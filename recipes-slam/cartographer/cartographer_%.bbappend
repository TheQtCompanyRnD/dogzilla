# meta-ros' jazzy cartographer bbappend is missing the use-newer-abseil-api
# patch that its kilted/rolling counterparts carry. Without it, cartographer's
# old unprefixed Abseil thread-safety macros (LOCKS_EXCLUDED, GUARDED_BY, ...)
# don't compile against the newer abseil-cpp in this release. Add the patch
# (vendored from meta-ros2-rolling; same cartographer 2.0.9004-1 sources).
FILESEXTRAPATHS:prepend := "${THISDIR}/${BPN}:"
SRC_URI += "file://use-newer-abseil-api.patch"

# meta-ros' cartographer bbappend forces CXXFLAGS += "-fuse-ld=gold" to work
# around static-liblua undefined dl* references. But binutils 2.45 in this
# Yocto release is built with --disable-gold (gold is deprecated upstream), so
# there is no ld.gold and the compiler test fails with "cannot find 'ld'".
# Drop the flag to fall back to ld.bfd. On modern glibc (2.34+) dlopen/dlsym
# live in libc, so the original gold workaround is no longer needed.
CXXFLAGS:remove = "-fuse-ld=gold"
