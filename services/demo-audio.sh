#!/usr/bin/env bash
# Optional robot -> laptop audio bridge, for recording demo videos where the
# robot's small USB speaker is too quiet. When "on", everything the robot plays
# (dogzillad's TTS, etc.) is tunneled over TCP to this laptop's PipeWire and
# comes out the laptop speakers instead; the robot speaker stays silent. "off"
# restores the robot's own speaker.
#
#   RUN THIS ON THE LAPTOP.
#
#   demo-audio.sh on     [robot]   # bridge robot audio to the laptop
#   demo-audio.sh off    [robot]   # restore the robot's own speaker
#   demo-audio.sh status [robot]   # show both ends
#
# [robot] is optional -- a hostname, an IP address, or user@either -- and
# defaults to pi@dogzilla.local, which relies on mDNS (avahi). If mDNS is not
# working, pass the robot's address instead:
#
#   demo-audio.sh on 169.254.8.23
#
# None of this involves the Internet, and none of it needs a routable address.
# A bare cable between laptop and robot, both ends falling back to IPv4
# link-local (169.254.x.x, see laptop-setup.sh), is a supported setup and the
# expected one at a venue.
#
# The laptop address the robot dials back to is whichever source address the
# kernel would use to reach the robot (`ip route get`), so it follows whatever
# link the robot is actually on. Override with LAPTOP_IP=...; if the robot's
# address can't be determined at all, you get a menu of this laptop's addresses.
# "on" waits up to $WAIT_SECS (default 40) for the robot to become addressable,
# since an IPv4 link-local address takes ~35s to appear after plugging in.
#
# ssh may prompt -- for a password (no key installed) or to accept a host key
# (a raw IP is not covered by the *.local ssh config). That is fine: all the
# connections in one run share a single multiplexed master, so at most one
# prompt per run.
#
# Mechanism, robot side, two pieces held open by transient systemd --user units
# (so on/off just start/stop them -- no PipeWire restart, and a running dogzillad
# keeps its audio):
#
#   1. `pw-cli -m load-module libpipewire-module-pulse-tunnel` -> a sink that
#      forwards over TCP to this laptop's pipewire-pulse (the laptop side is a
#      module-native-protocol-tcp server this script loads).
#   2. `pw-loopback` capturing the *robot speaker sink's monitor* and playing
#      into that tunnel sink, plus a mute on the robot speaker sink itself.
#
# Piece 2 is why we don't simply make the tunnel the default sink: an app that
# resolves the default output once at startup -- QtTextToSpeech's flite engine
# does, via QMediaDevices, and so dogzillad's voice does -- pins its stream to
# the sink that was default *then* and never follows a later default change. So
# instead of moving streams we leave everything playing to the robot speaker
# sink and tap its monitor: whatever a stream targeted, the audio lands in the
# tunnel. Muting that sink silences the speaker without silencing the monitor
# (PipeWire taps monitor ports ahead of the sink's volume/mute stage).
#
# The tunnel sink's volume is forced to 1.0 on every "on": WirePlumber restores
# whatever it was last set to, and a remembered low value (its own default is
# well under 10% linear) is otherwise indistinguishable from a broken bridge.
#
# Note that "off" is what unmutes the robot speaker, and WirePlumber remembers
# mute across reboots -- if the bridge dies with the laptop unplugged, run
# `demo-audio.sh off` (or `wpctl set-mute @DEFAULT_AUDIO_SINK@ 0` on the robot)
# to get the speaker back.
#
# Robot requirements: pw-cli, pw-loopback, wpctl and systemd-run (pipewire-tools
# + systemd; older dogzilla images that shipped only wpctl need a rebuild).

set -euo pipefail

ROBOT_DEFAULT="pi@dogzilla.local"
ROBOT="${ROBOT:-}"                      # resolved by resolve_robot
ROBOT_IP=""                             # robot's IPv4, when we could find it
TCP_PORT="${TCP_PORT:-4713}"
WAIT_SECS="${WAIT_SECS:-40}"            # how long "on" waits for the link
SINK_NAME="laptop_demo"                 # node.name of the tunnel sink
SINK_DESC="Laptop (demo)"               # node.description (shown in wpctl)
ROBOT_SPEAKER_DESC="USB Audio Device"   # substring of the robot's own sink
TUNNEL_UNIT="demo-audio-tunnel"         # transient user units on the robot
LOOPBACK_UNIT="demo-audio-loopback"

CHOSEN_IP=""       # laptop address the robot streams to, set by resolve_laptop_ip
CHOSEN_DEV=""      # ... and the interface it is on
CHOSEN_PREFIX=""   # ... and its prefix length, for the pulse ACL
# Robot-side launcher scripts (paths relative to $HOME).
TUNNEL_LAUNCHER='.cache/demo-audio-tunnel.sh'
LOOPBACK_LAUNCHER='.cache/demo-audio-loopback.sh'

# -x: no X11 forwarding (the robot rejects the channel and warns otherwise).
# LogLevel=ERROR: the *.local ssh config uses UserKnownHostsFile=/dev/null, which
# otherwise prints a "Permanently added" warning on every connection.
# No BatchMode: a password prompt is acceptable (there may be no key at the
# venue), and ControlMaster multiplexing means it is asked once per run rather
# than once per remote command. accept-new covers connecting by raw IP, which
# the *.local StrictHostKeyChecking exemption doesn't match.
SSH_OPTS=(-x -o LogLevel=ERROR -o ConnectTimeout=8
          -o StrictHostKeyChecking=accept-new
          -o ControlMaster=auto -o ControlPersist=60
          -o "ControlPath=${XDG_RUNTIME_DIR:-/tmp}/demo-audio-ssh-%r@%h:%p")

# Run a command on the robot under a login bash with the user PipeWire session.
# The remote login shell is fish; wrapping the whole command in a single-quoted
# `bash -lc '...'` keeps fish from re-parsing it (e.g. treating "(demo)" as a
# command substitution).
rssh() {
    ssh "${SSH_OPTS[@]}" "$ROBOT" \
        "bash -lc 'export XDG_RUNTIME_DIR=/run/user/\$(id -u); $*'"
}

# Resolve a hostname to one IPv4 address: nsswitch first (which is where
# mdns4_minimal normally hangs), then avahi directly in case it isn't wired in.
resolve_name() {
    local ip=""
    ip="$(getent ahostsv4 "$1" 2>/dev/null | awk '{print $1; exit}')"
    if [ -z "$ip" ] && command -v avahi-resolve-host-name >/dev/null 2>&1; then
        ip="$(avahi-resolve-host-name -4 "$1" 2>/dev/null | awk '{print $2; exit}')"
    fi
    printf '%s' "$ip"
}

# $1 = optional [robot] argument -> $ROBOT (ssh target) and $ROBOT_IP (may stay
# empty; ssh can still work by name even when we can't resolve it ourselves).
resolve_robot() {
    local spec="${1:-}"
    [ -n "$spec" ] || spec="${ROBOT:-$ROBOT_DEFAULT}"
    case "$spec" in
        *@*) ROBOT="$spec" ;;
        *)   ROBOT="${ROBOT_USER:-pi}@$spec" ;;
    esac

    local host="${ROBOT#*@}" real
    # ssh_config may rewrite the name (HostName ...); resolve what ssh will use.
    real="$(ssh -G "$host" 2>/dev/null | awk '$1=="hostname"{print $2; exit}')"
    [ -n "$real" ] && host="$real"
    case "$host" in
        [0-9]*.[0-9]*.[0-9]*.[0-9]*) ROBOT_IP="$host"; return ;;
    esac
    ROBOT_IP="$(resolve_name "$host")"
}

# Source address + interface the kernel would use to reach $1, tab separated.
route_src() {
    ip -4 route get "$1" 2>/dev/null | awk -v dst="$1" 'NR==1 {
        via = 0
        for (i = 1; i <= NF; i++) {
            if ($i == "dev") d = $(i+1)
            if ($i == "src") s = $(i+1)
            if ($i == "via") via = 1
        }
        # A link-local address reached "via" a gateway is a false positive: the
        # default route is answering for a 169.254 destination that has no link
        # of its own yet. Report no route, so we keep waiting for the real one.
        if (via && dst ~ /^169\.254\./) exit
        if (s != "") print s "\t" d
    }'
}

# Prefix length of address $1 on interface $2 (for the pulse ACL).
prefix_of() {
    ip -4 -o addr show dev "$2" 2>/dev/null \
        | awk -v a="$1" '{ split($4, p, "/"); if (p[1] == a) { print p[2]; exit } }'
}

# This laptop's IPv4 addresses, excluding loopback: "iface<TAB>addr<TAB>prefix".
# Deliberately not "scope global": on a bare cable both ends are link-local
# (169.254.x.x, scope link) and that is a perfectly good demo network.
laptop_addrs() {
    ip -4 -o addr show 2>/dev/null \
        | awk '$2 != "lo" { split($4, a, "/"); print $2 "\t" a[1] "\t" a[2] }'
}

# Wait (up to $WAIT_SECS) for the robot to be resolvable and routable: a
# link-local address takes ~35s to appear after a cable is plugged in, and
# dogzilla.local only resolves once the robot's avahi has announced on that link.
# Sets $ROBOT_IP / $CHOSEN_IP / $CHOSEN_DEV; 1 if the deadline passed first.
wait_for_robot() {
    local deadline=$(( SECONDS + WAIT_SECS )) said=0 r=""
    while :; do
        [ -n "$ROBOT_IP" ] || ROBOT_IP="$(resolve_name "${ROBOT#*@}")"
        if [ -n "$ROBOT_IP" ]; then
            r="$(route_src "$ROBOT_IP")"
            if [ -n "$r" ]; then
                CHOSEN_IP="$(printf '%s' "$r" | cut -f1)"
                CHOSEN_DEV="$(printf '%s' "$r" | cut -f2)"
                return 0
            fi
        fi
        [ "$SECONDS" -ge "$deadline" ] && return 1
        if [ "$said" -eq 0 ]; then
            echo "demo-audio: waiting up to ${WAIT_SECS}s for ${ROBOT#*@} to show up ..." >&2
            said=1
        fi
        sleep 2
    done
}

# Pick the laptop IP the robot should stream to -> $CHOSEN_IP/$CHOSEN_DEV/$CHOSEN_PREFIX.
resolve_laptop_ip() {
    if [ -n "${LAPTOP_IP:-}" ]; then
        CHOSEN_IP="$LAPTOP_IP"
        CHOSEN_DEV="$(laptop_addrs | awk -v a="$LAPTOP_IP" -F'\t' '$2==a{print $1; exit}')"
    elif wait_for_robot; then
        echo "demo-audio: robot ${ROBOT#*@} is ${ROBOT_IP} via ${CHOSEN_DEV}; laptop is ${CHOSEN_IP}" >&2
    else
        if [ -z "$ROBOT_IP" ]; then
            echo "demo-audio: can't resolve ${ROBOT#*@} (is avahi running on both ends?)." >&2
            echo "            Pass the robot's address: $(basename "$0") ${1:-on} <robot-ip>" >&2
        else
            echo "demo-audio: no route to ${ROBOT_IP} after ${WAIT_SECS}s -- is the cable in?" >&2
        fi
        choose_laptop_ip
    fi
    [ -n "$CHOSEN_DEV" ] && CHOSEN_PREFIX="$(prefix_of "$CHOSEN_IP" "$CHOSEN_DEV")"
    [ -n "$CHOSEN_PREFIX" ] || CHOSEN_PREFIX=24
}

# Last resort: ask which of this laptop's addresses the robot can reach.
choose_laptop_ip() {
    local lines
    mapfile -t lines < <(laptop_addrs)
    if [ "${#lines[@]}" -eq 0 ]; then
        echo "demo-audio: this laptop has no IPv4 address outside loopback." >&2
        echo "            Plug in the cable and wait for the link-local fallback," >&2
        echo "            or see laptop-setup.sh." >&2
        exit 1
    fi

    # Default to the most demo-like link: the cable (NAT subnet, then plain
    # link-local), and only then the lab/venue network.
    local def=1 best=0 i=1 ip score
    for l in "${lines[@]}"; do
        ip="$(printf '%s' "$l" | cut -f2)"
        case "$ip" in
            10.81.0.*)  score=3 ;;
            169.254.*)  score=2 ;;
            10.42.0.*)  score=1 ;;
            *)          score=0 ;;
        esac
        [ "$score" -gt "$best" ] && { best=$score; def=$i; }
        i=$((i+1))
    done

    echo "Select the laptop IP the robot should stream to:" >&2
    i=1
    for l in "${lines[@]}"; do
        printf "  %d) %-10s %s%s\n" "$i" \
            "$(printf '%s' "$l" | cut -f1)" "$(printf '%s' "$l" | cut -f2)" \
            "$([ "$i" -eq "$def" ] && echo '   (recommended)')" >&2
        i=$((i+1))
    done
    # Read from the terminal, not stdin -- but don't die where there is none
    # (a cron/CI/agent run): fall back to the recommended choice.
    local c=""
    if { exec 3</dev/tty; } 2>/dev/null; then
        read -rp "choice [$def]: " c <&3 || c=""
        exec 3<&-
    fi
    c="${c:-$def}"
    [ "$c" -ge 1 ] 2>/dev/null && [ "$c" -le "${#lines[@]}" ] || c="$def"
    CHOSEN_IP="$(printf '%s' "${lines[$((c-1))]}" | cut -f2)"
    CHOSEN_DEV="$(printf '%s' "${lines[$((c-1))]}" | cut -f1)"
}

# Fail early and legibly rather than halfway through the setup.
check_ssh() {
    if ! ssh "${SSH_OPTS[@]}" "$ROBOT" true; then
        echo "demo-audio: can't ssh to ${ROBOT}." >&2
        echo "            Try: ssh ${ROBOT}" >&2
        case "${ROBOT#*@}" in
            [0-9]*.[0-9]*.[0-9]*.[0-9]*) ;;
            *) echo "            ... or pass the robot's IP: $(basename "$0") ${1:-on} <robot-ip>" >&2 ;;
        esac
        exit 1
    fi
}

# Print the wpctl sink id whose line (in the Sinks: section) contains $1.
robot_sink_id() {
    rssh "wpctl status 2>/dev/null" | awk -v d="$1" '
        /Sinks:/   { s=1; next }
        /Sources:/ { s=0 }
        s && index($0, d) {
            for (i=1; i<=NF; i++) if ($i ~ /^[0-9]+\.$/) { sub(/\./,"",$i); print $i; exit }
        }'
}

# Print the node.name of the sink with wpctl id $1 (pw-loopback takes names).
robot_sink_node_name() {
    rssh "wpctl inspect $1 2>/dev/null" | sed -n 's/.*node\.name = "\(.*\)".*/\1/p' | head -1
}

# Ship a robot-side launcher: $1 = path relative to $HOME, $2 = command to exec.
# The SPA-JSON and quoted props (spaces, quotes, "(demo)" parens) can't survive
# being re-parsed by the remote fish login shell, so we build the script locally
# -- values expanded here -- and ship it as raw bytes over stdin.
ship_launcher() {
    printf '%s\n' '#!/bin/bash' "exec $2" \
        | ssh "${SSH_OPTS[@]}" "$ROBOT" \
            "bash -lc 'mkdir -p ~/.cache && cat > \"\$HOME/$1\" && chmod +x \"\$HOME/$1\"'"
}

# (Re)start transient unit $1 running launcher $2. Stopping the unit kills the
# process, which unloads its module/nodes; no PipeWire restart involved.
run_unit() {
    rssh "systemctl --user stop $1 2>/dev/null; systemctl --user reset-failed $1 2>/dev/null; systemd-run --user --unit=$1 --collect \$HOME/$2"
}

stop_unit() {
    rssh "systemctl --user stop $1 2>/dev/null || true; systemctl --user reset-failed $1 2>/dev/null || true"
}

# The tunnel sink: forwards everything written to it over TCP to the laptop.
robot_tunnel_up() {   # $1=laptop ip  $2=port
    ship_launcher "$TUNNEL_LAUNCHER" \
        "pw-cli -m load-module libpipewire-module-pulse-tunnel '{ tunnel.mode=sink pulse.server.address=\"tcp:$1:$2\" stream.props={ node.name=$SINK_NAME node.description=\"$SINK_DESC\" } }'"
    run_unit "$TUNNEL_UNIT" "$TUNNEL_LAUNCHER"
}

# The monitor tap: robot speaker sink's monitor -> tunnel sink. Captures what
# every stream plays to the speaker, including streams pinned to that sink.
robot_loopback_up() {   # $1=node.name of the robot speaker sink
    ship_launcher "$LOOPBACK_LAUNCHER" \
        "pw-loopback -C \"$1\" --capture-props='stream.capture.sink=true node.name=demo_speaker_tap node.description=\"Demo speaker tap\"' -P $SINK_NAME --playback-props='node.name=demo_tunnel_feed node.description=\"Demo tunnel feed\"'"
    run_unit "$LOOPBACK_UNIT" "$LOOPBACK_LAUNCHER"
}

# Ensure the laptop pipewire-pulse TCP server is listening with an ACL that
# admits the robot: its own address when we know it, plus the chosen interface's
# subnet (which on a link-local cable is 169.254.0.0/16, not a /24). A stale
# module from an earlier run on a different network is reloaded, not reused.
laptop_tcp_up() {
    local acl="${CHOSEN_IP}/${CHOSEN_PREFIX};127.0.0.1"
    [ -n "$ROBOT_IP" ] && acl="${ROBOT_IP}/32;${acl}"
    local old
    old="$(pactl list short modules \
        | awk -v p="port=${TCP_PORT}" '/module-native-protocol-tcp/ && index($0,p)')"
    if [ -n "$old" ]; then
        case "$old" in *"auth-ip-acl=${acl}"*) return ;; esac
        printf '%s\n' "$old" | awk '{print $1}' \
            | while read -r id; do pactl unload-module "$id" || true; done
    fi
    pactl load-module module-native-protocol-tcp \
        auth-anonymous=1 "auth-ip-acl=${acl}" "port=${TCP_PORT}" >/dev/null
}

laptop_tcp_down() {
    pactl list short modules \
        | awk -v p="port=${TCP_PORT}" '/module-native-protocol-tcp/ && index($0,p){print $1}' \
        | while read -r id; do pactl unload-module "$id" || true; done
}

case "${1:-}" in
on)
    resolve_robot "${2:-}"
    resolve_laptop_ip "on"
    check_ssh on
    spk="$(robot_sink_id "$ROBOT_SPEAKER_DESC")" || true
    if [ -z "$spk" ]; then
        echo "ERROR: no robot sink matching '$ROBOT_SPEAKER_DESC'; is the speaker plugged in?" >&2
        exit 1
    fi
    spk_name="$(robot_sink_node_name "$spk")"
    echo "Opening laptop pulse TCP server on :${TCP_PORT} (for ${CHOSEN_IP}) ..."
    laptop_tcp_up
    echo "Starting tunnel on ${ROBOT} -> ${CHOSEN_IP}:${TCP_PORT} ..."
    robot_tunnel_up "$CHOSEN_IP" "$TCP_PORT"
    # The sink appears a moment after pw-cli connects; poll for it.
    id=""
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        id="$(robot_sink_id "$SINK_DESC")" || true
        [ -n "$id" ] && break
        sleep 0.5
    done
    if [ -z "$id" ]; then
        echo "ERROR: tunnel sink '$SINK_DESC' did not appear on the robot." >&2
        echo "Is ${CHOSEN_IP}:${TCP_PORT} reachable from the robot? Check:" >&2
        echo "  ssh $ROBOT 'systemctl --user status ${TUNNEL_UNIT}'" >&2
        exit 1
    fi
    echo "Tapping the robot speaker's monitor into the tunnel ..."
    robot_loopback_up "$spk_name"
    # Keep the speaker sink the default so every stream -- default-following or
    # pinned -- flows through the tap; mute it so only the laptop is audible.
    rssh "wpctl set-default $spk 2>/dev/null; wpctl set-volume $id 1.0 2>/dev/null; wpctl set-mute $spk 1 2>/dev/null"
    if ! rssh "pw-link -l 2>/dev/null" | grep -q "demo_tunnel_feed"; then
        echo "ERROR: the monitor tap did not link up. Check:" >&2
        echo "  ssh $ROBOT 'systemctl --user status ${LOOPBACK_UNIT}'" >&2
        exit 1
    fi
    echo "ON: robot audio -> laptop (tunnel sink id $id). Robot speaker is muted."
    ;;
off)
    resolve_robot "${2:-}"
    check_ssh off
    echo "Unmuting robot speaker ..."
    spk="$(robot_sink_id "$ROBOT_SPEAKER_DESC")" || true
    [ -n "$spk" ] && rssh "wpctl set-default $spk 2>/dev/null; wpctl set-mute $spk 0 2>/dev/null" || true
    echo "Stopping tap + tunnel units on ${ROBOT} ..."
    stop_unit "$LOOPBACK_UNIT"
    stop_unit "$TUNNEL_UNIT"
    echo "Closing laptop pulse TCP server ..."
    laptop_tcp_down
    echo "OFF: robot plays through its own speaker again."
    ;;
status)
    resolve_robot "${2:-}"
    echo "=== robot ==="
    echo "${ROBOT}${ROBOT_IP:+  (${ROBOT_IP})}"
    echo "=== laptop pulse TCP server ==="
    pactl list short modules | grep "module-native-protocol-tcp.*port=${TCP_PORT}" || echo "(not loaded)"
    check_ssh status   # the laptop half is worth printing even when the robot is away
    echo "=== robot units (${TUNNEL_UNIT}, then ${LOOPBACK_UNIT}) ==="
    rssh "systemctl --user is-active ${TUNNEL_UNIT} ${LOOPBACK_UNIT} 2>/dev/null || true"
    echo "=== robot sinks (default = *; speaker should read MUTED when on) ==="
    rssh "wpctl status 2>/dev/null" | awk '/Sinks:/{s=1;next} /Sources:/{s=0} s'
    ;;
*)
    # The header comment, up to the first blank/non-comment line, is the usage.
    awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "$0"
    exit 1
    ;;
esac
