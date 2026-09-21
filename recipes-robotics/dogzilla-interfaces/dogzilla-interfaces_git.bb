SUMMARY = "Dogzilla custom ROS 2 interfaces (telemetry message)"
DESCRIPTION = "rosidl message package for the Dogzilla robot: msg/Telemetry \
(fan level + CPU load). Built from the dogzilla repo's dogzilla_interfaces/ \
subdir (DOGZILLA_GIT_REPO, set in meta-dogzilla/conf/layer.conf). The \
QtRos2 QML wrapper (Dogzilla.Telemetry) is generated downstream in dogzillad."
HOMEPAGE = "https://github.com/TheQtCompanyRnD/dogzilla"

# Our own interfaces; no upstream license file to checksum (matches dogzillad).
LICENSE = "CLOSED"

inherit ros_distro_jazzy

# Fetched over https from the upstream dogzilla repo (DOGZILLA_GIT_REPO and
# DOGZILLA_SRCREV are set in meta-dogzilla/conf/layer.conf)
# Bump DOGZILLA_SRCREV there to pick up new commits;
# `devtool modify dogzilla-interfaces` for live working-tree iteration.
# The package is the dogzilla_interfaces/ subdir of the repo.
SRC_URI = "git://${DOGZILLA_GIT_REPO};protocol=https;branch=main"
SRCREV = "${DOGZILLA_SRCREV}"
PV = "1.0+git"
S = "${UNPACKDIR}/${BP}/dogzilla_interfaces"

# rosidl message package: ament_cmake + the default rosidl generators (host
# tools via -native). No message-package build deps (Telemetry uses only int32/
# float32 primitives).
# Host-side rosidl generator TOOLS (run natively) ...
ROS_BUILDTOOL_DEPENDS = "ament-cmake-native rosidl-default-generators-native"
# ... plus the TARGET rosidl packages, so the aarch64 rosidl libs (e.g.
# librosidl_runtime_c) are staged in the recipe sysroot. Without these the
# cross link falls back to the native (x86_64) copy -> "file in wrong format".
# std-msgs: StampedTelemetry now leads with a std_msgs/Header (for the stamp),
# so the message package find_package(std_msgs)es it and needs it staged.
# action-msgs: Speak.action makes rosidl implicitly find_package(action_msgs)
# (goal/result wrappers use action_msgs/msg/GoalStatus etc.); without it staged,
# the lookup falls through to the native ros prefix (missing the .so) and ninja
# fails on libaction_msgs__rosidl_generator_c.so.
# trajectory-msgs: PlayMotion.action's goal embeds a trajectory_msgs/JointTrajectory,
# so the package find_package(trajectory_msgs)es it (same staging rationale as std-msgs).
ROS_BUILD_DEPENDS = "rosidl-default-generators type-description-interfaces std-msgs action-msgs trajectory-msgs rosidl-default-runtime"
ROS_EXEC_DEPENDS = "rosidl-default-runtime std-msgs action-msgs trajectory-msgs"
DEPENDS += "${ROS_BUILDTOOL_DEPENDS} ${ROS_BUILD_DEPENDS}"
RDEPENDS:${PN} += "${ROS_EXEC_DEPENDS}"

# rosidl_generate_interfaces() runs every installed generator, including the
# Rust one (rosidl_generator_rs), which aborts with KeyError if ROS_DISTRO isn't
# in the environment. It's a bitbake var but isn't exported into do_compile.
export ROS_DISTRO = "jazzy"

ROS_BUILD_TYPE = "ament_cmake"
inherit ros_${ROS_BUILD_TYPE}

# rosidl ships its typesupport as UNVERSIONED .so runtime libraries (no SONAME
# symlink), which OE's default split would put in -dev (dev-elf QA error, plus a
# main->-dev rdepends). Treat .so as the runtime SOLIB so they land in the main
# package instead.
SOLIBS = ".so"
FILES_SOLIBSDEV = ""
# ros_ament_cmake sets AMENT_PREFIX_PATH + the CMAKE_PREFIX_PATH cross flags
# (host ros_prefix for find_package), so we do NOT hand-roll those here.
