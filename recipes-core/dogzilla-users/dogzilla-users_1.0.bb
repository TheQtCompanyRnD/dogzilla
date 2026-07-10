SUMMARY = "Dogzilla 'pi' user: password, ssh authorized key, and sudo access"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://authorized_keys \
    file://pi.sudoers \
    file://home-pi \
"

inherit useradd

USERADD_PACKAGES = "${PN}"
# uid 1000, home /home/pi (created from skel via -m), fish login shell.
# Password "doggo" (SHA-512 crypt). Single-quoted so the shell in the useradd
# postinst does not treat the $-signs in the hash as variable expansions.
USERADD_PARAM:${PN} = "-u 1000 -d /home/pi -m -s /usr/bin/fish \
    -p '$6$r/wUsiOOafHZY7zi$snruyWuA6UquzqFK0Em98Mqb32.t7w/8aaVE9BnDVwlq3EkVRv.Y3rgiqjNfKFW3pZbvD34J5BaK/5ZaM2S.p/' \
    pi"

RDEPENDS:${PN} = "sudo bash fish"

do_install() {
    install -d ${D}/home/pi

    # Version-controlled home overlay: drop files under files/home-pi/ and they
    # land in /home/pi on every rebuild (dotfiles/config, etc.). cp -a to keep
    # nested dirs and any file modes.
    if [ -d ${UNPACKDIR}/home-pi ]; then
        cp -a ${UNPACKDIR}/home-pi/. ${D}/home/pi/
    fi

    install -d ${D}/home/pi/.ssh
    install -m 0600 ${UNPACKDIR}/authorized_keys ${D}/home/pi/.ssh/authorized_keys

    install -d ${D}${sysconfdir}/sudoers.d
    install -m 0440 ${UNPACKDIR}/pi.sudoers ${D}${sysconfdir}/sudoers.d/pi
}

# The files are packaged as root; OpenSSH refuses group/other-accessible key
# files and homes. The pi account exists by first boot (useradd runs at rootfs),
# so take ownership of the whole home (overlay + .ssh) and fix key perms.
pkg_postinst_ontarget:${PN} () {
    chown -R pi:pi /home/pi
    chmod 700 /home/pi/.ssh
    chmod 600 /home/pi/.ssh/authorized_keys
}

FILES:${PN} = " \
    /home/pi \
    ${sysconfdir}/sudoers.d/pi \
"
