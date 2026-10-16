# This libg2o does ship properly versioned solibs (libg2o_core.so.0.1.0 + .so
# symlink), but meta-ros inherits ros_faulty_solibs globally, which pulls
# ${ros_libdir}/lib*.so into the runtime package -- so every symlink trips
#   QA Issue: non -dev/-dbg/nativesdk- package libg2o contains symlink .so
#   '/opt/ros/jazzy/lib/libg2o_incremental.so' [dev-so]
# meta-ros' own bbappend tries to undo this with
# FILES:${PN}-dev += "${libdir}/libg2o*.so", but these libraries install under
# ${ros_libdir} (/opt/ros/jazzy/lib), not ${libdir}, so it matches nothing.
# -dev comes before ${PN} in PACKAGES, so claiming them here is enough.
FILES:${PN}-dev += "${ros_libdir}/libg2o*.so"
