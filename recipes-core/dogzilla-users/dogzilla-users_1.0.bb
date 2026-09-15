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
# Password is set separately in pkg_postinst_ontarget via chpasswd -e: the
# useradd postinst runs the -p value through an eval + flock -c re-parse
# that can strip the quoting around $-signs in a crypt hash (verified this
# mangled the "doggo" hash on this build host, into a shadow entry with the
# $6$..$ salt/algo markers silently deleted).
# -G groups give the pi user hardware access without sudo: dialout (serial ->
# motor controller + lidar UARTs), audio (mic/speaker), video (camera),
# input (gamepad/evdev), tty (ConsoleDashboard writes to /dev/tty1 directly).
# These exist in base-passwd at useradd time; render/plugdev are created
# dynamically at rootfs so can't be used here.
USERADD_PARAM:${PN} = "-u 1000 -d /home/pi -m -s /usr/bin/fish \
    -G dialout,audio,video,input,tty \
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
    # Password "doggo" (SHA-512 crypt). Set here via chpasswd -e rather than
    # useradd -p, since this runs as a single plain script (no eval/flock -c
    # re-parse) and the hash's $-signs reach chpasswd intact.
    echo 'pi:$6$r/wUsiOOafHZY7zi$snruyWuA6UquzqFK0Em98Mqb32.t7w/8aaVE9BnDVwlq3EkVRv.Y3rgiqjNfKFW3pZbvD34J5BaK/5ZaM2S.p/' | chpasswd -e

    chown -R pi:pi /home/pi
    chmod 700 /home/pi/.ssh
    chmod 600 /home/pi/.ssh/authorized_keys
}

FILES:${PN} = " \
    /home/pi \
    ${sysconfdir}/sudoers.d/pi \
"
