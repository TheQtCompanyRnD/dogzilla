SUMMARY = "Userspace driver for the Dogzilla's 128x32 SSD1306 OLED"
DESCRIPTION = "Draws a text file on the OLED on the dog's back, over /dev/i2c-1, \
and redraws it whenever it changes (inotify). dogzillad's ConsoleDashboard writes \
that file. Deliberately does NOT use the kernel's ssd130x framebuffer driver: that \
works, but panics ('scheduling while atomic') when the display's refresh collides \
with Wi-Fi interrupts on the Pi 5's RP1. From userspace a bus hiccup is an EIO. \
Plain C, no libraries, so it also builds on the robot itself with cc. \
See oledd/README.md in the dogzilla repo for the whole story."
HOMEPAGE = "https://github.com/TheQtCompanyRnD/dogzilla"

# Our own application; no upstream license file to checksum.
LICENSE = "CLOSED"

# Fetched over https from the upstream dogzilla repo (DOGZILLA_GIT_REPO and
# DOGZILLA_SRCREV are set in meta-dogzilla/conf/layer.conf)
# Bump DOGZILLA_SRCREV there to pick up new commits;
# `devtool modify oledd` for live working-tree iteration.
# oledd is the oledd/ subdir of the repo.
SRC_URI = "git://${DOGZILLA_GIT_REPO};protocol=https;branch=main"
SRCREV = "${DOGZILLA_SRCREV}"
PV = "1.0+git"
S = "${UNPACKDIR}/${BP}/oledd"

inherit systemd

# A system service, not a --user one like dogzillad: /dev/i2c-1 is root-owned
# and there is no i2c group on this image.
SYSTEMD_SERVICE:${PN} = "oledd.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_compile() {
    oe_runmake
}

# The Makefile's install target is for installing by hand (it puts the unit in
# /etc/systemd/system); a package belongs in ${systemd_system_unitdir}.
do_install() {
    install -D -m 0755 ${B}/oledd ${D}${bindir}/oledd
    install -D -m 0644 ${S}/oledd.service ${D}${systemd_system_unitdir}/oledd.service

    # /dev/i2c-1 only appears once the chardev driver is loaded; the kernel has
    # it as a module. dtparam=i2c_arm=on (RPI_EXTRA_CONFIG, in the kas config)
    # creates the bus itself.
    install -D -m 0644 ${S}/i2c-dev.conf ${D}${sysconfdir}/modules-load.d/i2c-dev.conf
}

FILES:${PN} += " \
    ${systemd_system_unitdir}/oledd.service \
    ${sysconfdir}/modules-load.d/i2c-dev.conf \
"
