SUMMARY = "Dogzilla NetworkManager connection profiles"
DESCRIPTION = "Pre-seeded (non-secret) NetworkManager connection profiles for \
the robot. Ships one wired profile that does DHCP when a server is present and \
falls back to link-local for a direct-to-laptop cable. Wi-Fi profiles (with \
credentials) are added on-device with nmcli and are not baked into the image."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://wired.nmconnection"

inherit allarch

RDEPENDS:${PN} = "networkmanager"

do_install() {
    install -d ${D}${sysconfdir}/NetworkManager/system-connections
    # NetworkManager ignores keyfiles that are group/world-readable.
    install -m 0600 ${UNPACKDIR}/wired.nmconnection \
        ${D}${sysconfdir}/NetworkManager/system-connections/wired.nmconnection
}

FILES:${PN} = "${sysconfdir}/NetworkManager/system-connections/wired.nmconnection"
