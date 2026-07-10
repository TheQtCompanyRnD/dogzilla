SUMMARY = "Dogzilla S2 robot image: headless ROS 2 Jazzy + Qt 6 daemons for Raspberry Pi 5."
DESCRIPTION = "${SUMMARY}"
LICENSE = "MIT"

# Build on top of meta-ros' core ROS image (found via BBPATH from
# meta-ros-common). Brings in core-image-minimal + ros-core and the ROS
# image/distro classes.
require recipes-core/images/ros-image-core.bb

# Enables `bitbake ros-image-dogzilla -c populate_sdk` to produce a cross-SDK
# that includes the Qt6 host tools (moc/rcc/uic/qmltyperegistrar/qml + qt-cmake)
# alongside the cross toolchain and the full target sysroot (Qt + ROS + the
# bridge). Use it on the laptop to build dogzillad (Core AND Quick) and rsync
# the binary to the Pi -- on-target Qt building isn't possible (no host tools).
inherit populate_sdk_qt6

# On-target development: gcc/g++/make, plus headers and -dev packages for the
# installed libraries; debug tools; and an ssh server for remote work.
IMAGE_FEATURES += "tools-sdk dev-pkgs tools-debug ssh-server-openssh"

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
"

# System services. rpi-resize-rootfs grows the rootfs to fill the SD card on
# first boot (the .wic image ships a rootfs partition sized to its contents).
DOGZILLA_SYSTEM = " \
    rpi-resize-rootfs \
    dogzilla-users \
    dogzilla-udev \
"

# Audio: PipeWire (+ wireplumber session manager, + pulse-compat server for
# QtMultimedia/Qt Speech). qtspeech provides QTextToSpeech via the flite engine
# (offline TTS). whisper.cpp provides offline speech-to-text (base.en model).
# NOTE: qtspeech pulls flite through its PACKAGECONFIG. Natural-voice TTS (piper)
# is deferred -- it needs an onnxruntime recipe (see notes).
DOGZILLA_AUDIO = " \
    pipewire \
    pipewire-pulse \
    wireplumber \
    qtspeech \
    whisper-cpp \
    whisper-cpp-models-base-en \
"

# Networking: NetworkManager owns ethernet/wifi handoff; avahi for mDNS.
# networkmanager-wifi is the Wi-Fi device plugin (pulls wpa-supplicant); without
# it nmcli can't drive any wireless interface. dogzilla-network-config ships the
# pre-seeded wired profile (DHCP + link-local fallback for a direct laptop cable).
DOGZILLA_NETWORK = " \
    networkmanager \
    networkmanager-nmcli \
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
    qt-ros2-bridge \
"

# ROS 2: SLAM via cartographer, and Python client library for ROS nodes.
DOGZILLA_ROS = " \
    cartographer-ros \
    rclpy \
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
