# Fix up ROS 2 interface packages that declare rosidl_default_generators as a
# plain <depend> instead of a <buildtool_depend>.
#
# superflore translates such a <depend> into the *target* recipe
# rosidl-default-generators, which then gets staged into the recipe sysroot.
# find_package() finds it there (ros_ament_cmake puts the target sysroot ahead
# of the native one in CMAKE_PREFIX_PATH) and its
# ament_cmake_export_dependencies-extras.cmake immediately fails:
#
#   By not providing "Findrosidl_core_generators.cmake" in CMAKE_MODULE_PATH
#   this project has asked CMake to find a package configuration file provided
#   by "rosidl_core_generators", but CMake did not find one.
#
# -- because rosidl-core-generators is a native-only recipe. Packages that get
# it right (ackermann-msgs and friends) list rosidl-default-generators-native
# in ROS_BUILDTOOL_DEPENDS and rosidl-default-runtime at runtime. ~130 of
# meta-ros' generated jazzy recipes get it wrong, so fix the class of bug
# rather than carrying a bbappend per package (seen on slam-toolbox and
# aruco-msgs so far).
#
# The ROS_*_DEPENDS variables are rewritten rather than DEPENDS/RDEPENDS,
# because the generated recipes build DEPENDS from them by lazy reference.
python __anonymous() {
    if bb.data.inherits_class('native', d) or bb.data.inherits_class('nativesdk', d):
        return

    generators = 'rosidl-default-generators'

    for var in ('ROS_BUILD_DEPENDS', 'ROS_EXPORT_DEPENDS'):
        value = d.getVar(var)
        if value and generators in value.split():
            d.setVar(var, ' '.join(x for x in value.split() if x != generators))
            d.appendVar('ROS_BUILDTOOL_DEPENDS', ' %s-native' % generators)

    value = d.getVar('ROS_EXEC_DEPENDS')
    if value and generators in value.split():
        d.setVar('ROS_EXEC_DEPENDS',
                 ' '.join(x if x != generators else 'rosidl-default-runtime'
                          for x in value.split()))
}
