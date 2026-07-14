SUMMARY = "Dogzilla NetworkManager connection profiles + ROS 2 network env"
DESCRIPTION = "Pre-seeded (non-secret) NetworkManager connection profiles for \
the robot. Ships one wired profile that does DHCP when a server is present and \
falls back to link-local for a direct-to-laptop cable. Wi-Fi profiles (with \
credentials) are added on-device with nmcli and are not baked into the image. \
Also sets ROS_DOMAIN_ID for interactive fish shells so ros2 CLI matches the \
dogzillad daemon (which sets it via its systemd unit)."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://wired.nmconnection \
           file://10-ros-domain.fish"

inherit allarch

RDEPENDS:${PN} = "networkmanager"

do_install() {
    install -d ${D}${sysconfdir}/NetworkManager/system-connections
    # NetworkManager ignores keyfiles that are group/world-readable.
    install -m 0600 ${UNPACKDIR}/wired.nmconnection \
        ${D}${sysconfdir}/NetworkManager/system-connections/wired.nmconnection

    # ROS_DOMAIN_ID for interactive fish sessions (see the .fish file). Keep in
    # sync with Environment=ROS_DOMAIN_ID in dogzillad.service.
    install -d ${D}${sysconfdir}/fish/conf.d
    install -m 0644 ${UNPACKDIR}/10-ros-domain.fish \
        ${D}${sysconfdir}/fish/conf.d/10-ros-domain.fish
}

FILES:${PN} = "${sysconfdir}/NetworkManager/system-connections/wired.nmconnection \
               ${sysconfdir}/fish/conf.d/10-ros-domain.fish"
