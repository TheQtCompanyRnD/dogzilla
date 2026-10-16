# GTSAM 4.2.0 still declares cmake_minimum_required(VERSION 3.0); CMake 4
# (whinlatter) removed compatibility with anything below 3.5:
#   Compatibility with CMake < 3.5 has been removed from CMake.
# Take CMake's own suggested escape hatch rather than patching the (many)
# cmake_minimum_required lines in the tree. Drop this when meta-ros ships a
# newer GTSAM.
EXTRA_OECMAKE += "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

# Boost 1.89 ships no compiled boost_system (header-only since 1.69), which
# GTSAM's HandleBoost.cmake still demands by name.
FILESEXTRAPATHS:prepend := "${THISDIR}/${BPN}:"
SRC_URI += "file://0001-do-not-ask-for-the-boost-system-component.patch"

# GTSAM ships versioned solibs (libgtsam.so.4.2.0 + .so symlink), but meta-ros
# inherits ros_faulty_solibs globally, which claims ${ros_libdir}/lib*.so for
# the runtime package -- so the symlinks trip
#   QA Issue: non -dev/-dbg/nativesdk- package gtsam contains symlink .so
#   '/opt/ros/jazzy/lib/libgtsam.so' [dev-so]
# -dev comes before ${PN} in PACKAGES, so claiming them here is enough.
# (Same fix as recipes-slam/libg2o.)
FILES:${PN}-dev += "${ros_libdir}/libgtsam*.so"

# Build against the Eigen in the sysroot instead of GTSAM's bundled copy.
# Otherwise every consumer that also uses system Eigen -- rtabmap here -- hits
#   gtsam/base/Vector.h: static assertion failed: Error: GTSAM was built
#   against a different version of Eigen
# meta-ros sets this in its own gtsam bbappend on the rolling and kilted
# branches; jazzy's bbappend is an older revision that lacks it (it also still
# points FILES:${PN}-dev at ${libdir}, hence the fix above).
EXTRA_OECMAKE += "-DGTSAM_USE_SYSTEM_EIGEN=ON"
