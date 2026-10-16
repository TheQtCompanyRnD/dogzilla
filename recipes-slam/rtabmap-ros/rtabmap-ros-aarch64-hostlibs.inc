# Shared by the rtabmap_ros packages that carry upstream's aarch64-only
# "make sure these libraries exist" guard (upstream issues #1285, #1288):
#
#   if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
#     find_library(<foo>_LIB NAMES <foo>
#       PATHS "/opt/ros/$ENV{ROS_DISTRO}/lib"
#       NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH REQUIRED)
#   endif()
#
# It hardcodes the *host's* ROS install path and explicitly refuses the CMake
# find-root, so under Yocto -- where every one of these libraries is present in
# the recipe sysroot -- it fails configure outright:
#   Could not find builtin_interfaces__rosidl_generator_c_LIB using the
#   following names: builtin_interfaces__rosidl_generator_c
#
# None of the resulting variables is used afterwards; the block is purely an
# existence check. Pre-seeding the cache entries makes find_library a no-op.
# Setting a variable a given package does not use is harmless, so one list
# covers them all (rtabmap-conversions, -msgs, -odom, -slam, -sync).
EXTRA_OECMAKE += " \
    -Dbuiltin_interfaces__rosidl_generator_c_LIB=${STAGING_DIR_HOST}${ros_libdir}/libbuiltin_interfaces__rosidl_generator_c.so \
    -Drcutils_LIB=${STAGING_DIR_HOST}${ros_libdir}/librcutils.so \
    -Dcrypto_LIB=${STAGING_DIR_HOST}${libdir}/libcrypto.so \
"
