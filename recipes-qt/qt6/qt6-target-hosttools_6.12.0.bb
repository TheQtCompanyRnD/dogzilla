SUMMARY = "Prebuilt aarch64 Qt 6 Core/Gui host tools for on-target builds"
DESCRIPTION = "\
The Qt code-generator tools moc/rcc/uic/syncqt + qtpaths/qmake and the \
Qt6{Core,Gui,DBus,Widgets}Tools CMake packages, as aarch64 binaries that run \
on the Pi. A cross Yocto build does NOT build these for the target \
(QT_FORCE_BUILD_TOOLS defaults OFF in qt6-cmake.bbclass, and forcing it ON \
contaminates the image's own Qt cross builds -- see the note in \
kas/dogzilla-raspberrypi5-jazzy.yml). So find_package(Qt6 Core) fails on the \
device with 'Could NOT find Qt6CoreTools'. These prebuilt tools fill exactly \
that gap so 'cmake' can build Qt apps natively on the robot. \
\
The QML tools (qmltyperegistrar/qmlcachegen/qml + Qt6Qml/QuickTools cmake) are \
NOT here -- qtdeclarative DOES build those for the target, so install the real \
'qtdeclarative-tools' package instead (this recipe deliberately excludes them \
to avoid file conflicts). \
\
Binaries were extracted from the aarch64 SDK (kas/dogzilla-sdk-aarch64.yml, \
i.e. SDKMACHINE=aarch64 + populate_sdk) and had their ELF interpreter \
relocated to the target loader. To regenerate after a Qt version bump: \
rebuild that SDK, take usr/libexec/{moc,rcc,uic,syncqt,...}, usr/bin/{qmake, \
qtpaths,qdbus*} and usr/lib/cmake/Qt6{Core,Gui,DBus,Widgets}Tools out of its \
sysroot, repoint each ELF interpreter at the target loader \
(patchelf --set-interpreter /lib/ld-linux-aarch64.so.1), and re-tar as usr/."

# Built from open-source Qt, so these binaries carry Qt's open-source terms:
# the tools themselves are GPL-3.0 with the Qt GPL exception (which is what
# keeps apps you moc/rcc/uic from inheriting the GPL), and the Qt libraries
# they link are the usual LGPL-3.0/GPL tri-license. Not CLOSED -- that would
# misstate the license of code we are redistributing in binary form.
#
# The tarball is an opaque prebuilt with no LICENSES/ dir of its own, so the
# checksums below reference oe-core's copies rather than files in ${S}.
# Qt-GPL-exception-1.0 has no oe-core copy; it resolves through the
# LICENSE_PATH that meta-qt6 adds, so it lands in the image license manifest.
#
# Redistributing LGPL binaries carries a source-availability obligation. The
# corresponding source is open-source Qt 6.12 at the qtbase SRCREV that
# meta-qt6 pins for this build -- keep that pin and this recipe in step.
LICENSE = "(GPL-3.0-only & Qt-GPL-exception-1.0) & (LGPL-3.0-only | GPL-2.0-only | GPL-3.0-only)"
LIC_FILES_CHKSUM = " \
    file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6 \
    file://${COMMON_LICENSE_DIR}/GPL-3.0-only;md5=c79ff39f19dfec6d293b95dea7b07891 \
    file://${COMMON_LICENSE_DIR}/LGPL-3.0-only;md5=bfccfe952269fff2b407dd11f2f3083b \
"

PV = "6.12.0"

SRC_URI = "file://qt6-target-hosttools-${PV}-aarch64.tar.gz"
# tarball holds a usr/ tree; unpack it into a subdir (modern OE forbids S=WORKDIR)
UNPACKDIR = "${WORKDIR}/sources"
S = "${UNPACKDIR}"

# aarch64 prebuilt: tie to the machine and skip QA that assumes we compiled it.
PACKAGE_ARCH = "${MACHINE_ARCH}"
COMPATIBLE_MACHINE = "raspberrypi5"
INSANE_SKIP:${PN} += "ldflags arch textrel already-stripped file-rdeps"

# Runtime libs the tools link (rcc/uic need libQt6Core; rcc needs libzstd).
RDEPENDS:${PN} += "qtbase"

do_install() {
    install -d ${D}${prefix}
    cp -a ${S}/usr/. ${D}${prefix}/
    # the tarball preserves the host build user's ownership; force root so
    # do_package_qa doesn't flag host-user-contaminated (runs under pseudo).
    chown -R root:root ${D}${prefix}
}

# Keep the CMake packages in the main package (not -dev): on-target builds need
# them present alongside the tools.
FILES:${PN} += "${libdir}/cmake"
