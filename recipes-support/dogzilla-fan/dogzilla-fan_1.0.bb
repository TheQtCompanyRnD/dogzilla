SUMMARY = "Silence the Pi 5 chassis fan during mic capture (dogzillad PTT helper)"
DESCRIPTION = "A small root helper that suspends the cpu-thermal zone's governor \
and forces the fan off ('quiet'), then restores automatic control ('auto'), so \
the fan (right next to the USB mic) doesn't swamp push-to-talk recordings. \
Includes a sudoers drop-in letting the pi user run it without a password."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://dogzilla-fan \
    file://dogzilla-fan.sudoers \
"

inherit allarch

# The sudoers drop-in grants the pi user password-less use of the helper.
RDEPENDS:${PN} = "sudo"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${UNPACKDIR}/dogzilla-fan ${D}${bindir}/dogzilla-fan

    # sudoers.d files must be 0440 root:root or sudo refuses to load them.
    install -d ${D}${sysconfdir}/sudoers.d
    install -m 0440 ${UNPACKDIR}/dogzilla-fan.sudoers ${D}${sysconfdir}/sudoers.d/dogzilla-fan
}

FILES:${PN} = " \
    ${bindir}/dogzilla-fan \
    ${sysconfdir}/sudoers.d/dogzilla-fan \
"
