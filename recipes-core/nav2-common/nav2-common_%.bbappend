# nav2_package() (the macro every navigation2 package calls first thing) adds
# -Werror, and GCC 15 then turns warnings from *sysroot* headers -- rclcpp's
# -Wshadow, rosidl_runtime_cpp's deprecated std::wstring_convert -- into build
# failures for nav2-util, nav2-amcl and friends. Patch the macro here, in the
# one package that defines it, rather than carrying a -Wno-error for each
# consumer. The warning flags themselves are kept.
FILESEXTRAPATHS:prepend := "${THISDIR}/${BPN}:"
SRC_URI += "file://0001-nav2_package-do-not-force-werror.patch"
