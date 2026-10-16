# meta-ros' own libpointmatcher bbappend adds
# 0001-Make-yaml-cpp-pm-use-cmake-clean.patch, which patches
# contrib/yaml-cpp-pm/CMakeLists.txt -- a bundled yaml-cpp copy that 1.4.2 no
# longer ships (it does find_package(yaml-cpp CONFIG REQUIRED) against the
# system one, which is already in DEPENDS). So the patch cannot apply:
#   can't find file to patch at input line 5 ... No file to patch
# Drop it. Remove this when meta-ros drops the patch for jazzy.
SRC_URI:remove = "file://0001-Make-yaml-cpp-pm-use-cmake-clean.patch"

# Boost 1.89 ships no compiled boost_system (header-only since 1.69), which
# libpointmatcher still asks for by name in a REQUIRED find_package.
FILESEXTRAPATHS:prepend := "${THISDIR}/${BPN}:"
SRC_URI += "file://0001-do-not-ask-for-the-boost-system-component.patch"

# Boost 1.89 also dropped the Filesystem v2 free functions (complete(),
# extension()) that IO.cpp still calls.
SRC_URI += "file://0002-stop-using-the-removed-boost-filesystem-v2-api.patch"
