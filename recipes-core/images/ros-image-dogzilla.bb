SUMMARY = "Dogzilla S2 robot image: headless ROS 2 Jazzy + Qt 6 daemons for Raspberry Pi 5."
DESCRIPTION = "${SUMMARY}"
LICENSE = "MIT"

# Build on top of meta-ros' core ROS image (found via BBPATH from
# meta-ros-common). Brings in core-image-minimal + ros-core and the ROS
# image/distro classes.
require recipes-core/images/ros-image-core.bb

# Enables `bitbake ros-image-dogzilla -c populate_sdk` to produce a cross-SDK
# with the Qt6 host tools (moc/rcc/uic/qmltyperegistrar/qml + qt-cmake) + cross
# toolchain + target sysroot (the image's Qt/ROS packages incl. -dev). Use it on
# the laptop to build dogzillad (Core AND Quick) and rsync to the Pi.
#
# We inherit the *base* Qt SDK class (env + qt-cmake toolchain file) and add only
# the host-tools packagegroup -- NOT the full populate_sdk_qt6, whose
# packagegroup-qt6-modules pulls qtdeviceutilities, which hard-RDEPENDS connman
# and conflicts with networkmanager in the SDK sysroot. dogzillad's Qt modules
# are already in the image sysroot, so the full module set isn't needed.
inherit populate_sdk_qt6_base
TOOLCHAIN_HOST_TASK:append = " nativesdk-packagegroup-qt6-toolchain-host"

# On-target development: gcc/g++/make, plus headers and -dev packages for the
# installed libraries; debug tools; and an ssh server for remote work.
# package-management installs opkg + the package DB so you can `opkg install`
# .ipk files on the device (e.g. a freshly rebuilt dogzillad) without reflashing.
IMAGE_FEATURES += "tools-sdk dev-pkgs tools-debug ssh-server-openssh package-management"

# This device only runs daemons, so no window system is installed. The Qt libs
# are built with gui support (qtdeclarative needs it) but nothing here pulls in
# a compositor or display server. See kas config for DISTRO_FEATURES.

IMAGE_INSTALL:append = " \
    ${DOGZILLA_NETWORK} \
    ${DOGZILLA_QT} \
    ${DOGZILLA_ROS} \
    ${DOGZILLA_PYTHON} \
    ${DOGZILLA_FIRMWARE} \
    ${DOGZILLA_SYSTEM} \
    ${DOGZILLA_DEV} \
    ${DOGZILLA_AUDIO} \
"

# On-device development: git + rsync support iterating on dogzillad without
# reflashing (build on host with the SDK/devtool and deploy over ssh, or build
# on target directly using the gcc/g++/cmake + -dev packages already installed).
# bash as an interactive login shell; sudo for the pi user.
DOGZILLA_DEV = " \
    git \
    rsync \
    bash \
    fish \
    sudo \
    cmake \
    qtbase-tools \
    qt6-target-hosttools \
    qtdeclarative-tools \
"

# On-target Qt builds: find_package(Qt6) needs the code-gen tools + Qt6*Tools
# cmake, which a cross image lacks. qtdeclarative DOES build its tools for the
# target, so qtdeclarative-tools supplies qml/qmltyperegistrar/qmlcachegen +
# (via qtdeclarative-dev from dev-pkgs) Qt6Qml/QuickTools cmake. qtbase does NOT
# build moc/rcc/uic for the target (QT_FORCE_BUILD_TOOLS=OFF; forcing it ON
# breaks the image's own cross build), so qt6-target-hosttools ships those +
# Qt6Core/GuiTools cmake as prebuilt aarch64 binaries. Together they let plain
# `cmake` build Qt apps natively on the Pi. (ROS 2 nodes additionally need the
# ament build system + -dev files -- tracked separately.)

# System services. rpi-resize-rootfs grows the rootfs to fill the SD card on
# first boot (the .wic image ships a rootfs partition sized to its contents).
DOGZILLA_SYSTEM = " \
    rpi-resize-rootfs \
    dogzilla-users \
    dogzilla-udev \
    dogzilla-fan \
"

# Audio: PipeWire (+ wireplumber session manager, + pulse-compat server for
# QtMultimedia/Qt Speech). qtspeech provides QTextToSpeech via the flite engine
# (offline TTS). whisper.cpp provides offline speech-to-text (base.en model).
# NOTE: qtspeech pulls flite through its PACKAGECONFIG. Natural-voice TTS (piper)
# is deferred -- it needs an onnxruntime recipe (see notes).
DOGZILLA_AUDIO = " \
    pipewire \
    pipewire-pulse \
    pipewire-spa-plugins-alsa \
    pipewire-alsa \
    pipewire-alsa-card-profile \
    wireplumber \
    alsa-utils \
    qtspeech \
    whisper-cpp \
    whisper-cpp-models-tiny-en-q5-1 \
    whisper-cpp-models-base-en \
"
# Two STT models installed side by side (pick at runtime with whisper-cli -m):
# tiny.en-q5_1 (~32 MB, ~2x faster) as the fast default, base.en f16 (~148 MB)
# as the higher-accuracy fallback. Model size (tiny vs base) drives speed on the
# A76; quantization mainly shrinks footprint. No usable GPU on the Pi 5.
# pipewire-spa-plugins-alsa: PipeWire's ALSA device backend -- REQUIRED for the
#   USB sound card to appear as a sink/source (without it: only "Dummy Output").
# pipewire-alsa(-card-profile): route ALSA-API apps through PipeWire + proper
#   card profiles/ports (output/input, mic) via ACP/UCM.
# alsa-utils: speaker-test / aplay / arecord / amixer for hardware bring-up and
#   testing the speaker + USB mic at the raw ALSA layer (below PipeWire).

# Networking: NetworkManager owns ethernet/wifi handoff; avahi for mDNS.
# networkmanager-wifi is the Wi-Fi device plugin (pulls wpa-supplicant); without
# it nmcli can't drive any wireless interface. dogzilla-network-config ships the
# pre-seeded wired profile (DHCP + link-local fallback for a direct laptop cable).
DOGZILLA_NETWORK = " \
    networkmanager \
    networkmanager-nmcli \
    networkmanager-nmtui \
    networkmanager-wifi \
    dogzilla-network-config \
    avahi-daemon \
    avahi-utils \
"

# Qt 6.12 runtime modules requested for the robot daemons.
DOGZILLA_QT = " \
    qtbase \
    qtdeclarative \
    qtserialport \
    qtmultimedia \
    qtgamepad \
    qtgamepad-qmlplugins \
    qtgamepad-plugins \
    qt-ros2-bridge \
"

# ROS 2: SLAM via cartographer, and Python client library for ROS nodes.
DOGZILLA_ROS = " \
    cartographer-ros \
    rclpy \
    dogzillad \
    dogzilla-slam \
"

# Python 3 runtime. numpy + pyserial are used by the Dogzilla-provided scripts
# (struct/time are in python3-core). Add more python3-* modules here as other
# scripts need them.
DOGZILLA_PYTHON = " \
    python3 \
    python3-core \
    python3-pip \
    python3-numpy \
    python3-pyserial \
"

# Onboard Cypress/Infineon 43455 wifi + BCM4345C0 BT firmware for the Pi 5,
# plus Ralink firmware (rt2870.bin) for the RT5370 USB wifi dongle (rt2800usb).
DOGZILLA_FIRMWARE = " \
    linux-firmware-rpidistro-bcm43455 \
    bluez-firmware-rpidistro-bcm4345c0-hcd \
    linux-firmware-ralink \
"
