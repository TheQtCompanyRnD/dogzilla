# meta-ros' cartographer bbappend forces CXXFLAGS += "-fuse-ld=gold" to work
# around static-liblua undefined dl* references. But binutils 2.45 in this
# Yocto release is built with --disable-gold (gold is deprecated upstream), so
# there is no ld.gold and the compiler test fails with "cannot find 'ld'".
# Drop the flag to fall back to ld.bfd. On modern glibc (2.34+) dlopen/dlsym
# live in libc, so the original gold workaround is no longer needed.
CXXFLAGS:remove = "-fuse-ld=gold"
