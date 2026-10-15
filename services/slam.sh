#!/bin/bash
# Run one SLAM backend: slam.sh cartographer | slam_toolbox
. /opt/ros/jazzy/setup.bash
# Resolve the launch + config files relative to this script, so they only need
# to be deployed together (see the Makefile).
DIR="$(dirname "$(readlink -f "$0")")"
exec ros2 launch "$DIR/${1:?usage: slam.sh cartographer|slam_toolbox}.launch.py"
