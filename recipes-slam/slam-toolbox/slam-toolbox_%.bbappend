# Build slam_toolbox on this headless image.
#
# meta-ros blacklists slam-toolbox for us (jazzy ros-distro-recipe-blacklist.inc):
#   "qt5: depends on qtbase; x11: depends on rviz-common->rviz-ogre-vendor->
#    (libx11,libxrandr,libxaw) which require x11 in DISTRO_FEATURES"
# Both skip groups are active here: there is no meta-qt5 layer (we are on Qt 6)
# and the robot has no window system (DISTRO_FEATURES:remove = "x11 wayland").
#
# All of that is for ONE thing: the SlamToolboxPlugin RViz panel, which upstream
# wires into the top-level CMakeLists with no way to turn it off. The actual SLAM
# nodes use neither rviz nor Qt. So patch the panel out and drop the deps, and
# the package builds headless. Visualization happens off-robot (run RViz on the
# laptop against the robot's topics) -- the panel is only a convenience GUI for
# the serialize/deserialize/pause services, which are plain ROS services.

FILESEXTRAPATHS:prepend := "${THISDIR}/${BPN}:"
SRC_URI += "file://0001-build-headless-drop-the-rviz-panel-plugin.patch"

# Boost 1.89 (whinlatter) ships no compiled boost_system library, which the
# upstream find_package(Boost REQUIRED system ...) still asks for by name.
SRC_URI += "file://0002-do-not-ask-for-the-boost-system-component.patch"

# Un-blacklist. The upstream flag is set with ?= in the ros-distro include, but
# that include is parsed before this bbappend, so a plain assignment here wins.
SKIP_RECIPE[slam-toolbox] = ""

# Match the patched CMakeLists: no rviz, no Qt5. (qtbase here is the superflore
# mapping of slam_toolbox's package.xml <depend>qtbase</depend>, i.e. Qt5-for-the
# -panel; the image's Qt 6 qtbase is unrelated and stays.) package.xml keeps its
# now-unused rviz_* <depend> lines -- nothing reads them at build or run time.
ROS_BUILD_DEPENDS:remove = "qtbase rviz-common rviz-default-plugins rviz-ogre-vendor rviz-rendering"
ROS_EXPORT_DEPENDS = ""
ROS_EXEC_DEPENDS:remove = "qtbase rviz-common rviz-default-plugins rviz-ogre-vendor rviz-rendering"

# NOTE: slam_toolbox also declares rosidl_default_generators as a plain
# <depend> rather than a <buildtool_depend>, which breaks do_configure. That is
# not fixed here: it affects ~130 meta-ros jazzy recipes, so it is handled
# centrally by classes/ros-rosidl-generators-fixup.bbclass.

# CMakeLists does find_package(PkgConfig REQUIRED) but package.xml never
# declares pkg-config, so nothing puts pkg-config-native in the sysroot:
#   Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)
inherit pkgconfig

# ceres-solver builds only a static library and headers, so there is no runtime
# ceres-solver package and do_rootfs fails with
#   nothing provides ceres-solver needed by slam-toolbox
# The solver is linked in statically; the runtime dependency is spurious.
# (meta-ros does the same for cartographer: "Doesn't need runtime dependency on
# ceres-solver".)
ROS_EXEC_DEPENDS:remove = "ceres-solver"
