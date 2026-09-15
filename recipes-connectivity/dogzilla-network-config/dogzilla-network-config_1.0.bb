SUMMARY = "Dogzilla wired-network config + ROS 2 network env"
DESCRIPTION = "Wired networking for the robot: systemd-networkd does DHCP on \
eth0 and falls back to an IPv4 link-local address for a direct-to-laptop cable, \
and NetworkManager is told to leave eth0 alone so the two don't race. \
NetworkManager keeps the wifi radios -- Wi-Fi profiles (with credentials) are \
added on-device with nmcli/nmtui and are not baked into the image. A \
networkd-dispatcher hook restarts avahi-daemon on every eth0 address change, \
so dogzilla.local keeps resolving across DHCP/link-local transitions. Also \
sets ROS_DOMAIN_ID for interactive fish shells so ros2 CLI matches the \
dogzillad daemon (which sets it via its systemd unit)."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://80-wired.network \
           file://10-networkd-owns-wired.conf \
           file://50-avahi-kick \
           file://10-ros-domain.fish"

inherit allarch

RDEPENDS:${PN} = "networkmanager networkd-dispatcher avahi-daemon"

do_install() {
    # Shadows systemd's own /usr/lib/systemd/network/80-wired.network, which has
    # no LinkLocalAddressing= and so leaves eth0 with no IPv4 at all when there
    # is no DHCP server (a cable straight to the laptop).
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${UNPACKDIR}/80-wired.network \
        ${D}${sysconfdir}/systemd/network/80-wired.network

    # ...and keep NetworkManager off eth0, or it runs a second DHCP client there
    # and its failed activations tear down networkd's addressing.
    install -d ${D}${sysconfdir}/NetworkManager/conf.d
    install -m 0644 ${UNPACKDIR}/10-networkd-owns-wired.conf \
        ${D}${sysconfdir}/NetworkManager/conf.d/10-networkd-owns-wired.conf

    # Re-announce over mDNS whenever eth0 gains or changes an address (DHCP
    # lease, IPv4LL fallback, or the handoff between the two) instead of
    # relying on avahi's own netlink monitor, which hasn't been reliable here.
    install -d ${D}${sysconfdir}/networkd-dispatcher/routable.d
    install -d ${D}${sysconfdir}/networkd-dispatcher/degraded.d
    install -m 0755 ${UNPACKDIR}/50-avahi-kick \
        ${D}${sysconfdir}/networkd-dispatcher/routable.d/50-avahi-kick
    install -m 0755 ${UNPACKDIR}/50-avahi-kick \
        ${D}${sysconfdir}/networkd-dispatcher/degraded.d/50-avahi-kick

    # ROS_DOMAIN_ID for interactive fish sessions (see the .fish file). Keep in
    # sync with Environment=ROS_DOMAIN_ID in dogzillad.service.
    install -d ${D}${sysconfdir}/fish/conf.d
    install -m 0644 ${UNPACKDIR}/10-ros-domain.fish \
        ${D}${sysconfdir}/fish/conf.d/10-ros-domain.fish
}

FILES:${PN} = "${sysconfdir}/systemd/network/80-wired.network \
               ${sysconfdir}/NetworkManager/conf.d/10-networkd-owns-wired.conf \
               ${sysconfdir}/networkd-dispatcher/routable.d/50-avahi-kick \
               ${sysconfdir}/networkd-dispatcher/degraded.d/50-avahi-kick \
               ${sysconfdir}/fish/conf.d/10-ros-domain.fish"
