SUMMARY = "Dogzilla S2 robot control daemon (Qt 6 + ROS 2)"
DESCRIPTION = "Headless daemon driving the Dogzilla S2: Qt6 (Core/Qml/SerialPort/\
UniversalInput) for control logic + the qt-ros2-bridge (Qt6Ros2Core) for ROS 2 \
(rclcpp) integration. Built in-tree by bitbake so the full ROS 2 build system \
(ament_cmake/rosidl, which is -native/host-side only) is assembled correctly -- \
a plain SDK/on-target CMake build can't find it. Iterate with devtool: \
'devtool modify dogzillad', edit, 'devtool build dogzillad', \
'devtool deploy-target dogzillad pi@dogzilla.local'."
HOMEPAGE = "https://github.com/TheQtCompanyRnD/dogzilla"

# BSD-3-Clause, per the LICENSE at the root of the dogzilla repo. S points at
# a subdir of that repo, hence the ../ here.
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://../LICENSE;md5=e7df2cb5b828712253329843cc2d34ca"

# Fetched over https from the upstream dogzilla repo (DOGZILLA_GIT_REPO and
# DOGZILLA_SRCREV are set in meta-dogzilla/conf/layer.conf)
# Bump DOGZILLA_SRCREV there to pick up new commits;
# `devtool modify dogzillad` for live working-tree iteration.
# The daemon is the dogzillad/ subdir of the repo, so point S at it.
SRC_URI = "git://${DOGZILLA_GIT_REPO};protocol=https;branch=main \
           file://dogzillad.service"
SRCREV = "${DOGZILLA_SRCREV}"
PV = "1.0+git"
S = "${UNPACKDIR}/${BP}/dogzillad"

# Qt side (host tools moc/rcc/qmltyperegistrar via *-native); ROS side (Jazzy
# env + python rosidl tooling) -- mirrors the qt-ros2-bridge recipe.
inherit qt6-cmake ros_distro_jazzy python3native pkgconfig

DEPENDS += " \
    qtbase \
    qtdeclarative \
    qtdeclarative-native \
    qtserialport \
    qtgamepad \
    qtmultimedia \
    qt-ros2-bridge \
    pulseaudio \
    whisper-cpp \
    dogzilla-interfaces \
    rclcpp \
    type-description-interfaces \
    rosidl-default-generators \
    rosidl-default-generators-native \
    ament-cmake-native \
    python3-native \
"

# Make the ROS 2 packages discoverable by find_package() during the cross build
# (mirrors qt-ros2-bridge / meta-ros' ros_ament_cmake.bbclass). Exported so it
# augments the prefix path qt6-cmake sets for finding Qt.
export AMENT_PREFIX_PATH = "${STAGING_DIR_HOST}${prefix};${STAGING_DIR_HOST}${ros_prefix};${STAGING_DIR_NATIVE}${prefix};${STAGING_DIR_NATIVE}${ros_prefix}"
export CMAKE_PREFIX_PATH = "${STAGING_DIR_HOST}${ros_prefix}:${STAGING_DIR_HOST}${prefix}"

EXTRA_OECMAKE += "-DBUILD_TESTING=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF"

# The app's CMakeLists has no install() rules, so install the built artifacts
# ourselves: the executable, and the QML module plugin/qmldir if produced.
do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/dogzillad ${D}${bindir}/dogzillad

    # main.qml and prompt.txt are shipped as editable files (kept scriptable, not baked into
    # the binary). dogzillad resolves it via QStandardPaths::AppDataLocation,
    # which searches ${datadir}/dogzillad (= /usr/share/dogzillad) as well as
    # ~/.local/share/dogzillad (a writable per-user override).
    install -d ${D}${datadir}/dogzillad
    install -m 0644 ${S}/qml/main.qml ${D}${datadir}/dogzillad/main.qml
    install -m 0644 ${S}/qml/prompt.txt ${D}${datadir}/dogzillad/prompt.txt

    # qt_add_qml_module may emit a QML plugin tree under the build dir; install
    # it if present (harmless no-op if the QML is compiled into the binary).
    if [ -d ${B}/Dogzilla ]; then
        install -d ${D}${libdir}/qml/Dogzilla
        cp -a ${B}/Dogzilla/. ${D}${libdir}/qml/Dogzilla/
    fi

    # systemd --user unit for the pi user, statically enabled via the global
    # user default.target.wants. A user unit only starts at boot if the user
    # lingers, so also mark pi as lingering (equivalent to
    # `loginctl enable-linger pi`). pi is created by dogzilla-users.
    install -d ${D}${systemd_user_unitdir}/default.target.wants
    install -m 0644 ${UNPACKDIR}/dogzillad.service ${D}${systemd_user_unitdir}/dogzillad.service
    ln -sf ../dogzillad.service ${D}${systemd_user_unitdir}/default.target.wants/dogzillad.service
    install -d ${D}${localstatedir}/lib/systemd/linger
    touch ${D}${localstatedir}/lib/systemd/linger/pi

    # cp -a preserves the host build user's ownership; force root so
    # do_package_qa doesn't flag host-user-contaminated (runs under pseudo).
    chown -R root:root ${D}
}

# whisper-cpp: libwhisper/libggml at runtime (dogzillad links them for STT).
# qtmultimedia + the whisper model (tiny.en-q5_1) are already in the image via
# DOGZILLA_QT / DOGZILLA_AUDIO. Likewise main.qml's TextToSpeech (offline flite
# TTS) resolves the QtTextToSpeech QML module + flite backend at runtime, carried
# by DOGZILLA_AUDIO (qtspeech) -- not RDEPENDed here because those are dynamically
# named subpackages (libqt6texttospeech-qmlplugins etc.) that aren't statically
# RPROVIDEd at parse time, same as we rely on the image for qtmultimedia.
RDEPENDS:${PN} += "dogzilla-users whisper-cpp dogzilla-interfaces"

# The generated Dogzilla.Interfaces QML wrapper (from qtros2_generate_from_package)
# is a static plugin (.a) that gets linked into the dogzillad executable, but its
# module dir (qmldir + the static .a) is also installed under /usr/lib/qml. The
# .a is a harmless leftover; allow it rather than splitting a -staticdev package.
INSANE_SKIP:${PN} += "staticdev"

FILES:${PN} += " \
    ${libdir}/qml \
    ${datadir}/dogzillad \
    ${systemd_user_unitdir} \
    ${localstatedir}/lib/systemd/linger/pi \
"

# NOTE: DEPENDS is a best-effort starting point. If do_configure fails with
# "Could not find a package configuration file provided by <pkg>", add the
# matching meta-ros recipe to DEPENDS (same as the qt-ros2-bridge recipe note).
