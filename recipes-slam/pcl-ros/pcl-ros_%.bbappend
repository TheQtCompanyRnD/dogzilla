# meta-ros' pcl-ros bbappend silences a buildpaths QA warning by deleting the
# sysroot prefix from the installed export_pcl_rosExport.cmake outright. That
# leaves the exported targets pointing at absolute paths that exist only inside
# a sysroot, so consumers (rtabmap-util is how we hit it) fail first at
# configure time:
#   Imported target "pcl_ros::pcl_ros_tf" includes non-existent path
#     "/usr/include/pcl-1.13"
# and then at link time:
#   ninja: error: '/usr/lib/libpcl_common.so', needed by
#     'librtabmap_util_plugins.so', missing and no known rule to make it
#
# Their do_install:append has already run by the time ours does -- this layer
# has the higher priority, so it is appended last -- so repair the result
# rather than prevent it: put CMake's own sysroot variable back in front of
# every absolute path in the export file, which each consumer resolves to its
# own sysroot (where these headers and libraries really are). Paths appear
# inside ;-separated quoted lists, hence the leading " or ; in the match.
do_install:append() {
    sed -i -E -e "s#([\";])(/usr/|${ros_prefix}/)#\1\$\{CMAKE_SYSROOT\}\2#g" \
        ${D}${ros_datadir}/pcl_ros/cmake/export_pcl_rosExport.cmake
}
