#!/usr/bin/env bash
# Set up an Ubuntu laptop to talk to the Dogzilla robot -- over a lab switch, a
# venue wifi, or a cable plugged straight between the two.
#
#   RUN THIS ON THE LAPTOP (Ubuntu 24.04 + NetworkManager).
#
#   laptop-setup.sh check   # report what's set and what's missing (default)
#   laptop-setup.sh apply   # make the changes (sudo only for one file)
#
# The robot side needs nothing: the meta-dogzilla image already ships it. There,
# systemd-networkd owns eth0 (DHCP, falling back to an IPv4 link-local address
# when no DHCP server answers), NetworkManager is told to leave eth0 alone and
# keeps only the wifi radios, avahi answers to dogzilla.local, and ROS_DOMAIN_ID
# is baked into dogzillad.service. So everything below is laptop-side.
#
# Three ways the two ends can be connected, and what happens:
#
#   Switch / venue wifi  Both take DHCP, mDNS gives dogzilla.local. Nothing to do.
#   Bare cable           Laptop DHCP fails -> the `cable-ll` profile takes a
#                        169.254.x address; the robot does the same by itself.
#                        Works with nothing typed, ~35s after plugging in. Good
#                        for ssh + ROS; no internet for the robot.
#   Cable + NAT          `nmcli con up robot-cable` -> the laptop serves DHCP on
#                        10.81.0.0/24 and masquerades, so the robot gets an
#                        address in ~5s plus internet through the laptop.
#
# Why `robot-cable` is not autoconnect: a `shared` profile that comes up by
# itself makes your laptop a rogue DHCP server on any venue LAN whose own DHCP
# is slow or absent. The link-local fallback covers the same "no server" case
# harmlessly, so bring shared up deliberately when you want NAT.
#
# The two non-obvious settings, both learned the hard way:
#
#   ipv4.dad-timeout 0   With wifi and ethernet on one L2 segment, the laptop's
#                        *own* wifi answers the duplicate-address ARP probe sent
#                        out ethernet, and NM counts that self-reply as a
#                        conflict and fails the activation.
#   dhcp-broadcast       The robot's DHCP DISCOVER carries its previous address
#                        as the source, so shared-mode dnsmasq unicasts the
#                        OFFER somewhere the client can't receive it and DHCP
#                        never completes ("IP configuration could not be
#                        reserved"). Broadcasting the reply fixes it.
#
# Note that ipv4.link-local=enabled on the DHCP profile is NOT a substitute for
# the separate cable-ll profile: it adds an address alongside a config that
# succeeded, but a DHCP timeout still fails the activation and drops all of IPv4.
#
# Right after a link switch, dogzilla.local can still resolve to the robot's old
# address. avahi on the robot re-announces by itself -- it watches netlink and
# doesn't care which daemon configured the interface -- but taking the link down
# can lose that announcement, leaving the querying end on its cached record until
# the ~120s mDNS TTL expires. Restart avahi-daemon on either end to skip the
# wait. `resolvectl flush-caches` won't help if /etc/nsswitch.conf resolves
# .local via mdns4_minimal (i.e. avahi) rather than through systemd-resolved.

set -euo pipefail

DOMAIN_ID="${DOMAIN_ID:-81}"           # must match the robot's dogzillad.service
# Laptop's address in shared mode. The 81 matches ROS_DOMAIN_ID (decimal ASCII
# 'Q'), purely as a mnemonic. What does matter is that this subnet not overlap
# the network the laptop is otherwise on: NM's `shared` default is 10.42.0.0/24,
# and against a venue/lab 10.42.0.0/16 the /24 is more specific, so the laptop
# routes the robot's *wifi* address out the dead cable and breaks both.
CABLE_SUBNET="${CABLE_SUBNET:-10.81.0.1/24}"
LL_PROFILE="cable-ll"
SHARED_PROFILE="robot-cable"
DNSMASQ_CONF="/etc/NetworkManager/dnsmasq-shared.d/broadcast.conf"
ROS_ENV="$HOME/.config/environment.d/90-ros-network.conf"

DEV="${DEV:-}"          # ethernet device; auto-detected when empty
DHCP_PROFILE=""         # the plain-DHCP profile for $DEV

note() { printf '  %s\n' "$*"; }
ok()   { printf '  ok   %s\n' "$*"; }
todo() { printf '  TODO %s\n' "$*"; }

# First ethernet device NetworkManager knows about.
find_device() {
    [ -n "$DEV" ] && return
    DEV="$(nmcli -t -f DEVICE,TYPE device status | awk -F: '$2=="ethernet"{print $1; exit}')"
    [ -n "$DEV" ] || { echo "no ethernet device found (set DEV=...)" >&2; exit 1; }
}

# The existing DHCP profile for $DEV, if any -- Ubuntu's is usually called
# netplan-<dev> or "Wired connection 1", so don't guess at the name.
find_dhcp_profile() {
    DHCP_PROFILE="$(nmcli -t -f NAME,TYPE connection show \
        | awk -F: '$2=="802-3-ethernet"{print $1}' \
        | while read -r name; do
            case "$name" in "$LL_PROFILE"|"$SHARED_PROFILE") continue;; esac
            ifn="$(nmcli -g connection.interface-name connection show "$name" 2>/dev/null || true)"
            m="$(nmcli -g ipv4.method connection show "$name" 2>/dev/null || true)"
            if [ "$m" = auto ] && { [ "$ifn" = "$DEV" ] || [ -z "$ifn" ]; }; then
                printf '%s' "$name"; break
            fi
        done)"
}

prop() {   # prop <profile> <property> -> value, "-" if the profile is missing
    nmcli -g "$2" connection show "$1" 2>/dev/null || printf '-'
}

do_check() {
    find_device
    find_dhcp_profile
    echo "ethernet device: $DEV"

    echo "DHCP profile (plugged into a switch):"
    if [ -z "$DHCP_PROFILE" ]; then
        todo "no plain-DHCP ethernet profile -- 'apply' will create one named 'wired'"
    else
        note "name: $DHCP_PROFILE"
        [ "$(prop "$DHCP_PROFILE" connection.autoconnect)" = yes ] \
            && ok "autoconnect yes" || todo "autoconnect is not yes"
        [ "$(prop "$DHCP_PROFILE" ipv4.dhcp-timeout)" = 15 ] \
            && ok "dhcp-timeout 15 (falls through to $LL_PROFILE quickly)" \
            || todo "dhcp-timeout is not 15"
        [ "$(prop "$DHCP_PROFILE" ipv4.dad-timeout)" = 0 ] \
            && ok "dad-timeout 0 (no self-ARP conflict when dual-homed)" \
            || todo "dad-timeout is not 0"
        [ "$(prop "$DHCP_PROFILE" connection.autoconnect-retries)" = 1 ] \
            && ok "autoconnect-retries 1 (one failure, then fall through)" \
            || todo "autoconnect-retries is not 1"
    fi

    echo "$LL_PROFILE (bare cable, nothing typed):"
    if [ "$(prop "$LL_PROFILE" ipv4.method)" = link-local ]; then
        ok "exists, ipv4.method link-local, priority $(prop "$LL_PROFILE" connection.autoconnect-priority)"
    else
        todo "missing"
    fi

    echo "$SHARED_PROFILE (cable + NAT, activated by hand):"
    if [ "$(prop "$SHARED_PROFILE" ipv4.method)" = shared ]; then
        ok "exists, $(prop "$SHARED_PROFILE" ipv4.addresses), autoconnect $(prop "$SHARED_PROFILE" connection.autoconnect)"
    else
        todo "missing"
    fi
    [ -f "$DNSMASQ_CONF" ] && ok "$DNSMASQ_CONF" || todo "$DNSMASQ_CONF missing (shared-mode DHCP will hang)"

    echo "ROS 2:"
    local envid=""
    [ -f "$ROS_ENV" ] && envid="$(awk -F= '/^ROS_DOMAIN_ID=/{print $2}' "$ROS_ENV" | tail -1)"
    if [ -z "$envid" ]; then
        todo "$ROS_ENV has no ROS_DOMAIN_ID"
    elif [ "$envid" = "$DOMAIN_ID" ]; then
        ok "$ROS_ENV: ROS_DOMAIN_ID=$envid"
    else
        todo "$ROS_ENV says $envid, robot uses $DOMAIN_ID -- systemd user services and desktop-launched apps inherit this file, so they'd be on the wrong domain"
    fi
    if [ "${ROS_DOMAIN_ID:-}" = "$DOMAIN_ID" ]; then
        ok "this shell: ROS_DOMAIN_ID=$DOMAIN_ID"
    else
        todo "this shell has ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-<unset>}, wanted $DOMAIN_ID (set it in your shell rc)"
    fi
    [ -z "${RMW_IMPLEMENTATION:-}" ] \
        && ok "RMW_IMPLEMENTATION unset (both ends must agree: Jazzy's default Fast DDS)" \
        || todo "RMW_IMPLEMENTATION=$RMW_IMPLEMENTATION is set -- unset it"
    if ip -4 -o addr show docker0 >/dev/null 2>&1; then
        todo "docker0 is up: Fast DDS binds every interface and wastes discovery on it"
    else
        ok "no docker0 in the way"
    fi
    echo "robot:"
    if getent hosts dogzilla.local >/dev/null 2>&1; then
        ok "dogzilla.local -> $(getent hosts dogzilla.local | awk '{print $1}')"
    else
        note "dogzilla.local does not resolve (robot off, or no mDNS -- needs avahi + libnss-mdns)"
    fi
}

do_apply() {
    find_device
    find_dhcp_profile

    if [ -z "$DHCP_PROFILE" ]; then
        nmcli connection add type ethernet con-name wired ifname "$DEV" ipv4.method auto >/dev/null
        DHCP_PROFILE=wired
    fi
    # Fail fast and only once, so NM moves on to the link-local profile.
    nmcli connection modify "$DHCP_PROFILE" \
        connection.autoconnect yes connection.autoconnect-priority 10 \
        connection.autoconnect-retries 1 ipv4.dhcp-timeout 15 ipv4.dad-timeout 0
    echo "configured DHCP profile: $DHCP_PROFILE ($DEV)"

    if [ "$(prop "$LL_PROFILE" ipv4.method)" != link-local ]; then
        nmcli connection delete "$LL_PROFILE" >/dev/null 2>&1 || true
        nmcli connection add type ethernet con-name "$LL_PROFILE" ifname "$DEV" \
            ipv4.method link-local ipv6.method link-local \
            connection.autoconnect yes connection.autoconnect-priority -10 >/dev/null
    fi
    echo "configured $LL_PROFILE (autoconnect fallback)"

    if [ "$(prop "$SHARED_PROFILE" ipv4.method)" != shared ]; then
        nmcli connection delete "$SHARED_PROFILE" >/dev/null 2>&1 || true
        nmcli connection add type ethernet con-name "$SHARED_PROFILE" ifname "$DEV" \
            ipv4.method shared ipv4.addresses "$CABLE_SUBNET" \
            connection.autoconnect no >/dev/null
    fi
    echo "configured $SHARED_PROFILE ($CABLE_SUBNET, activate by hand)"

    if [ ! -f "$DNSMASQ_CONF" ]; then
        echo "installing $DNSMASQ_CONF (needs sudo) ..."
        sudo install -d "$(dirname "$DNSMASQ_CONF")"
        sudo tee "$DNSMASQ_CONF" >/dev/null <<'EOF'
# Broadcast shared-mode DHCP replies. The robot's DHCP client sends DISCOVER
# with its previous address as the source and without the broadcast flag, so
# dnsmasq unicasts the OFFER to an address the client no longer owns and DHCP
# never completes. Installed by dogzilla/services/laptop-setup.sh.
dhcp-broadcast
EOF
    fi

    # Don't clobber an existing file (it may carry your own notes) -- fix only
    # the domain line, which is the part that has to match the robot.
    mkdir -p "$(dirname "$ROS_ENV")"
    if [ ! -f "$ROS_ENV" ]; then
        cat > "$ROS_ENV" <<EOF
# Must match the robot: Environment=ROS_DOMAIN_ID in dogzillad.service.
# Deliberately no RMW_IMPLEMENTATION -- both ends use Jazzy's default Fast DDS,
# which binds every interface and so follows whichever link is up.
ROS_DOMAIN_ID=$DOMAIN_ID
EOF
        echo "wrote $ROS_ENV (ROS_DOMAIN_ID=$DOMAIN_ID)"
    else
        cur="$(awk -F= '/^ROS_DOMAIN_ID=/{print $2}' "$ROS_ENV" | tail -1)"
        if [ -z "$cur" ]; then
            printf 'ROS_DOMAIN_ID=%s\n' "$DOMAIN_ID" >> "$ROS_ENV"
            echo "appended ROS_DOMAIN_ID=$DOMAIN_ID to $ROS_ENV"
        elif [ "$cur" != "$DOMAIN_ID" ]; then
            sed -i "s/^ROS_DOMAIN_ID=.*/ROS_DOMAIN_ID=$DOMAIN_ID/" "$ROS_ENV"
            echo "$ROS_ENV: ROS_DOMAIN_ID $cur -> $DOMAIN_ID (run 'systemctl --user daemon-reload', or re-login, to pick it up)"
        fi
    fi
    echo
    echo "environment.d covers systemd user services and graphical sessions, not"
    echo "plain shells -- add this to your shell rc as well:"
    echo "    export ROS_DOMAIN_ID=$DOMAIN_ID       # bash/zsh"
    echo "    set -gx ROS_DOMAIN_ID $DOMAIN_ID      # fish"
    echo
    do_check
}

case "${1:-check}" in
check) do_check ;;
apply) do_apply ;;
*) sed -n '2,54p' "$0"; exit 1 ;;
esac
