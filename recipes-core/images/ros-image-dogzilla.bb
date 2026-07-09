SUMMARY = "Dogzilla S2 robot image: headless ROS 2 Jazzy + Qt 6 daemons for Raspberry Pi 5."
DESCRIPTION = "${SUMMARY}"
LICENSE = "MIT"

# Build on top of meta-ros' core ROS image (found via BBPATH from
# meta-ros-common). Brings in core-image-minimal + ros-core and the ROS
# image/distro classes.
require recipes-core/images/ros-image-core.bb

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
"

# Networking: NetworkManager owns ethernet/wifi handoff; avahi for mDNS.
DOGZILLA_NETWORK = " \
    networkmanager \
    networkmanager-nmcli \
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

# Python 3 runtime.
DOGZILLA_PYTHON = " \
    python3 \
    python3-core \
    python3-pip \
"

# Onboard Broadcom/Cypress 43455 wifi+BT firmware for the Pi 5.
DOGZILLA_FIRMWARE = " \
    linux-firmware-rpidistro-bcm43455 \
    bluez-firmware-rpidistro-bcm43455 \
"
