#!/usr/bin/env bash
# Optional robot -> laptop audio bridge, for recording demo videos where the
# robot's small USB speaker is too quiet. When "on", everything the robot plays
# (dogzillad's TTS, etc.) is tunneled over TCP to this laptop's PipeWire and
# comes out the laptop speakers instead; the robot speaker stays silent. "off"
# restores the robot's own speaker.
#
#   RUN THIS ON THE LAPTOP. Requires passwordless ssh to $ROBOT.
#
#   demo-audio.sh on       # bridge robot audio to the laptop
#   demo-audio.sh off      # restore the robot's own speaker
#   demo-audio.sh status   # show both ends
#
# The laptop IP the robot dials back to is auto-detected: if this laptop has one
# global IPv4 it is used, otherwise you're prompted to pick the interface (the
# robot's 10.42.0.x shared link is offered as the default). Override with
# LAPTOP_IP=... to skip detection.
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

ROBOT="${ROBOT:-pi@dogzilla.local}"
TCP_PORT="${TCP_PORT:-4713}"
SINK_NAME="laptop_demo"                 # node.name of the tunnel sink
SINK_DESC="Laptop (demo)"               # node.description (shown in wpctl)
ROBOT_SPEAKER_DESC="USB Audio Device"   # substring of the robot's own sink
TUNNEL_UNIT="demo-audio-tunnel"         # transient user units on the robot
LOOPBACK_UNIT="demo-audio-loopback"

CHOSEN_IP=""   # set by resolve_laptop_ip
# Robot-side launcher scripts (paths relative to $HOME).
TUNNEL_LAUNCHER='.cache/demo-audio-tunnel.sh'
LOOPBACK_LAUNCHER='.cache/demo-audio-loopback.sh'

# -x: no X11 forwarding (the robot rejects the channel and warns otherwise).
# LogLevel=ERROR: the *.local ssh config uses UserKnownHostsFile=/dev/null, which
# otherwise prints a "Permanently added" warning on every connection.
SSH_OPTS=(-x -o BatchMode=yes -o LogLevel=ERROR)

# Run a command on the robot under a login bash with the user PipeWire session.
# The remote login shell is fish; wrapping the whole command in a single-quoted
# `bash -lc '...'` keeps fish from re-parsing it (e.g. treating "(demo)" as a
# command substitution).
rssh() {
    ssh "${SSH_OPTS[@]}" "$ROBOT" \
        "bash -lc 'export XDG_RUNTIME_DIR=/run/user/\$(id -u); $*'"
}

# Pick the laptop IP the robot should stream to -> $CHOSEN_IP.
resolve_laptop_ip() {
    if [ -n "${LAPTOP_IP:-}" ]; then CHOSEN_IP="$LAPTOP_IP"; return; fi

    local lines
    mapfile -t lines < <(ip -4 -o addr show scope global 2>/dev/null \
        | awk '{ split($4, a, "/"); print $2 "\t" a[1] }')
    [ "${#lines[@]}" -gt 0 ] || { echo "demo-audio: no global IPv4 on this laptop" >&2; exit 1; }

    if [ "${#lines[@]}" -eq 1 ]; then
        CHOSEN_IP="$(printf '%s' "${lines[0]}" | cut -f2)"
        echo "demo-audio: using $(printf '%s' "${lines[0]}" | tr '\t' ' ')" >&2
        return
    fi

    # Prefer the robot's 10.42.0.x shared link as the default choice.
    local def=1 i=1 ip
    for l in "${lines[@]}"; do
        ip="$(printf '%s' "$l" | cut -f2)"
        case "$ip" in 10.42.0.*) def=$i;; esac
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
    local c=""
    read -rp "choice [$def]: " c </dev/tty || c=""   # no tty -> take default
    c="${c:-$def}"
    CHOSEN_IP="$(printf '%s' "${lines[$((c-1))]}" | cut -f2)"
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

# Ensure the laptop pipewire-pulse TCP server is listening (idempotent). The ACL
# allows the robot's subnet (derived from the chosen laptop IP) plus loopback.
laptop_tcp_up() {
    local acl="${CHOSEN_IP%.*}.0/24;127.0.0.1"
    if ! pactl list short modules | grep -q "module-native-protocol-tcp.*port=${TCP_PORT}"; then
        pactl load-module module-native-protocol-tcp \
            auth-anonymous=1 "auth-ip-acl=${acl}" "port=${TCP_PORT}" >/dev/null
    fi
}

laptop_tcp_down() {
    pactl list short modules \
        | awk -v p="port=${TCP_PORT}" '/module-native-protocol-tcp/ && index($0,p){print $1}' \
        | while read -r id; do pactl unload-module "$id" || true; done
}

case "${1:-}" in
on)
    resolve_laptop_ip
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
    echo "=== laptop pulse TCP server ==="
    pactl list short modules | grep "module-native-protocol-tcp.*port=${TCP_PORT}" || echo "(not loaded)"
    echo "=== robot units (${TUNNEL_UNIT}, then ${LOOPBACK_UNIT}) ==="
    rssh "systemctl --user is-active ${TUNNEL_UNIT} ${LOOPBACK_UNIT} 2>/dev/null || true"
    echo "=== robot sinks (default = *; speaker should read MUTED when on) ==="
    rssh "wpctl status 2>/dev/null" | awk '/Sinks:/{s=1;next} /Sources:/{s=0} s'
    ;;
*)
    sed -n '2,48p' "$0"
    exit 1
    ;;
esac
