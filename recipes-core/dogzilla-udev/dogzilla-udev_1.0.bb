SUMMARY = "Dogzilla udev rules: /dev/lidar and /dev/motor symlinks, I2C access"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://99-dogzilla-serial.rules"

inherit allarch

do_install() {
    install -d ${D}${sysconfdir}/udev/rules.d
    install -m 0644 ${UNPACKDIR}/99-dogzilla-serial.rules \
        ${D}${sysconfdir}/udev/rules.d/99-dogzilla-serial.rules
}

FILES:${PN} = "${sysconfdir}/udev/rules.d/99-dogzilla-serial.rules"
