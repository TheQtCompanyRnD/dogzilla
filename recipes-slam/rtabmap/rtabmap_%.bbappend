# Build RTAB-Map's core library on this headless image.
#
# meta-ros blacklists rtabmap for jazzy (ros-distro-recipe-blacklist.inc):
#   "opengl/x11: Depends on libfreenect and libg2o which is not available
#    because of missing opengl/x11 in DISTRO_FEATURES;
#    openni: Depends on unavailable ROS_UNRESOLVED_DEP-libopenni-dev"
# but for jazzy that message is stale: the 0.22.1-1 recipe depends on neither
# libfreenect nor libopenni, and meta-ros' own libg2o bbappend already gates
# OpenGL behind a PACKAGECONFIG (G2O_USE_OPENGL=OFF without the opengl feature).
# What is left is qt-gui-cpp, the Qt 5 GUI binding, which we neither have nor
# want -- RTAB-Map's GUI (rtabmap-viz) is a separate package we do not install.
#
# meta-ros DOES carry a working bbappend for exactly this recipe version on the
# rolling and kilted branches; jazzy just never got it. This is that bbappend,
# trimmed to jazzy's dependency list, plus the un-blacklisting. Keep it in sync
# if meta-ros fixes jazzy upstream (then this file can go away).
SKIP_RECIPE[rtabmap] = ""

LICENSE = "BSD-3-Clause"

FILESEXTRAPATHS:prepend := "${THISDIR}/${BPN}:"
SRC_URI += "file://0001-Use-cmake-Thread-package-to-find-proper-threading-li.patch"

# qt-gui-cpp is Qt 5 (rtabmap's GUI); we build the library only, with -DWITH_QT=OFF.
DEPENDS:remove = "qt-gui-cpp"
RDEPENDS:${PN}:remove = "qt-gui-cpp"

# the native package only builds the res-tool, so strip everything heavy from
# it. libpointmatcher and gtsam are in jazzy's dependency list (they are not in
# the rolling/kilted recipes this is based on) and have no -native variant at
# all, so leaving them in gives "Nothing RPROVIDES libpointmatcher-native".
RTABMAP_NATIVE_DEPS_TO_REMOVE = "octomap libg2o cv-bridge pcl proj libpointmatcher gtsam"
DEPENDS:remove:class-native = "${RTABMAP_NATIVE_DEPS_TO_REMOVE}"
RDEPENDS:${PN}:remove:class-native = "${RTABMAP_NATIVE_DEPS_TO_REMOVE}"

# When we build for the target, we require a native build
DEPENDS:append:class-target = " ${BPN}-native"

# only build res-tool for native variant
OECMAKE_TARGET_COMPILE:class-native = "res_tool"

# Define was removed from PCL library in version 1.10:
CXXFLAGS += "-DDISABLE_VTK"

# Build without Qt, examples and tools. Build as release.
EXTRA_OECMAKE += " \
    -DBUILD_TOOLS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DWITH_QT=OFF \
    -DWITH_FREENECT=OFF \
    -DWITH_FREENECT2=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCOMPILER_SUPPORTS_CXX17=ON \
"

EXTRA_OECMAKE:class-native += " \
    -DUNIX=ON \
    -DANDROID_PREBUILD=ON \
"

export LD_LIBRARY_PATH = "${RECIPE_SYSROOT_NATIVE}/${ros_libdir}"

# QA: non -dev/-dbg package rtabmap contains symlink .so librtabmap_core.so etc.
# Undo ros_faulty_solibs and include the uncommon cmake config path:
# The version-suffixed cmake dir is rtabmap-0.22 for 0.22.1 (meta-ros' own
# bbappends still say 0.20/0.21); glob it so a version bump does not silently
# misfile the cmake package again.
FILES:${PN}-dev += " \
    ${ros_libdir}/${PN}-0.*/ \
    ${ros_libdir}/lib*${SOLIBSDEV} \
"

# QA: RTABMap_coreTargets.cmake contains a reference to TMPDIR [buildpaths].
# meta-ros' bbappend deletes the sysroot prefix outright, which leaves the
# exported target pointing at a bare "/usr/include/opencv4" -- a path that does
# not exist from a consumer's point of view, so rtabmap-conversions then dies
# with 'Imported target "rtabmap::core" includes non-existent path'. Substitute
# CMake's own sysroot variable instead: it resolves, per consumer, to that
# recipe's sysroot.
do_install:append:class-target() {
    sed -i -e "s#${RECIPE_SYSROOT}#$\{CMAKE_SYSROOT\}#g" ${D}${ros_libdir}/rtabmap-0.*/RTABMap_coreTargets.cmake
}

BBCLASSEXTEND = "native"

# Boost 1.89 ships no compiled boost_system (header-only since 1.69), which
# rtabmap still asks for by name in two REQUIRED find_package calls. (Same
# story as recipes-slam/libpointmatcher and recipes-slam/gtsam.)
SRC_URI += "file://0002-do-not-ask-for-the-boost-system-component.patch"
