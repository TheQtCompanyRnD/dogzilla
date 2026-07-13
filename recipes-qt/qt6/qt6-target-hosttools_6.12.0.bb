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
Binaries were extracted from the aarch64 SDK (SDKMACHINE=aarch64 nativesdk \
qtbase) and had their ELF interpreter relocated to the target loader. To \
regenerate after a Qt version bump, rebuild the aarch64 SDK and re-run the \
extract+patchelf steps (documented in project memory)."

# Prebuilt Qt binaries (upstream Qt is GPL-3.0/LGPL-3.0); packaged here as an
# opaque prebuilt, so CLOSED to skip per-file license checksumming.
LICENSE = "CLOSED"

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
