SUMMARY = "Expand the root filesystem to fill the storage device on first boot"
DESCRIPTION = "A systemd oneshot service that, on first boot, grows the root \
partition to fill the SD card (or other boot medium) and online-resizes the \
ext4 filesystem, then disables itself via a stamp file."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://resize-rootfs.sh \
    file://resize-rootfs.service \
"

inherit systemd features_check allarch

# Uses a systemd oneshot service.
REQUIRED_DISTRO_FEATURES = "systemd"

SYSTEMD_SERVICE:${PN} = "resize-rootfs.service"

# parted -> parted + partprobe; resize2fs -> e2fsprogs-resize2fs;
# findmnt -> util-linux-findmnt.
RDEPENDS:${PN} = "parted e2fsprogs-resize2fs util-linux-findmnt"

do_install() {
    install -d ${D}${sbindir}
    install -m 0755 ${UNPACKDIR}/resize-rootfs.sh ${D}${sbindir}/resize-rootfs.sh
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/resize-rootfs.service ${D}${systemd_system_unitdir}/resize-rootfs.service
}
