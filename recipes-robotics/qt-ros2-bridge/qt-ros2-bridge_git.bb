SUMMARY = "Qt ROS 2 bridge (QtRos2) module"
DESCRIPTION = "Qt module bridging Qt/QML with ROS 2 (rclcpp), built as a Qt \
BuildInternals CMake module that finds the ROS 2 packages in the sysroot."
HOMEPAGE = "https://git.qt.io/qt-robotics/qt-ros2-bridge"
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = " \
    file://LICENSES/BSD-3-Clause.txt;md5=cb40fa7520502d8c7a3aea47cae1316c \
    file://LICENSES/GPL-3.0-only.txt;md5=d32239bcb673463ab874e80d47fae504 \
    file://LICENSES/LicenseRef-Qt-Commercial.txt;md5=40a1036f91cefc0e3fabad241fb5f187 \
"

# Fetch from the local working copy over the filesystem (protocol=file) so no
# SSH connection to git.qt.io is made -- avoids a YubiKey touch on every fetch.
# QT_ROS2_BRIDGE_SRC is set in meta-dogzilla/conf/layer.conf; override it if
# your checkout lives elsewhere. AUTOREV tracks the tip of the branch's last
# *commit* (not uncommitted working-tree edits -- use devtool/externalsrc for
# live iteration). Pin to a SHA for reproducible builds.
# destsuffix and S are left at their oe-core defaults (${UNPACKDIR}/${BP});
# modern oe-core errors if a recipe still sets S = "${WORKDIR}/git".
SRC_URI = "git://${QT_ROS2_BRIDGE_SRC};protocol=file;branch=qtify"
SRCREV = "${AUTOREV}"
PV = "6.8.0+git"

# Qt side: qt_build_repo module needing Qt6 Core + Quick.
inherit qt6-cmake

# ROS side: pull in the Jazzy environment (AMENT_PREFIX_PATH etc.) and the
# Python tooling used by the rosidl code generator.
inherit ros_distro_jazzy python3native

DEPENDS += " \
    qtbase \
    qtdeclarative \
    qtdeclarative-native \
    rclcpp \
    rclcpp-action \
    tf2-geometry-msgs \
    rosidl-default-generators \
    rosidl-default-generators-native \
    ament-cmake-native \
    python3-native \
"

# ROS 2 interface packages the bridge generates Qt bindings from. This is the
# exact set of SOURCE_PACKAGE values across src/*/CMakeLists.txt; missing any
# fails do_configure in qtros2_generate_from_package().
DEPENDS += " \
    action-msgs builtin-interfaces composition-interfaces \
    diagnostic-msgs geometry-msgs lifecycle-msgs nav-msgs \
    rcl-interfaces rosgraph-msgs sensor-msgs service-msgs \
    shape-msgs statistics-msgs std-msgs std-srvs stereo-msgs \
    tf2-msgs trajectory-msgs type-description-interfaces \
    unique-identifier-msgs visualization-msgs \
"

# Make the ROS 2 packages discoverable by find_package() during the Qt CMake
# build. Mirrors what meta-ros' ros_ament_cmake.bbclass sets up. CMAKE_PREFIX_PATH
# is exported (rather than passed via -D) so it augments, instead of replacing,
# the prefix path that qt6-cmake configures for finding Qt.
export AMENT_PREFIX_PATH = "${STAGING_DIR_HOST}${prefix};${STAGING_DIR_HOST}${ros_prefix};${STAGING_DIR_NATIVE}${prefix};${STAGING_DIR_NATIVE}${ros_prefix}"
export CMAKE_PREFIX_PATH = "${STAGING_DIR_HOST}${ros_prefix}:${STAGING_DIR_HOST}${prefix}"

EXTRA_OECMAKE += "-DBUILD_TESTING=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF"

# Package like a Qt module. Runtime: the versioned libs (default FILES) plus the
# QML modules. Dev artifacts (headers, cmake, pkgconfig, .so symlinks) mostly go
# to -dev by default; add the Qt extras (.prl, metatypes, module json, mkspecs,
# sbom) that no default FILES covers. QML plugins ship unversioned .so under
# ${libdir}/qml (runtime), so skip the dev-so QA check for the main package.
FILES:${PN} += "${libdir}/qml"
FILES:${PN}-dev += " \
    ${libdir}/*.prl \
    ${libdir}/metatypes \
    ${libdir}/modules \
    ${libdir}/mkspecs \
    ${libdir}/sbom \
"
INSANE_SKIP:${PN} += "dev-so"

# NOTE: This is a novel Qt-module + ROS 2 cross build; the DEPENDS list above
# is a best-effort starting point. If do_configure fails with "Could not find
# a package configuration file provided by <pkg>", add the matching meta-ros
# recipe (e.g. rclcpp-action -> rclcpp-action) to DEPENDS.
