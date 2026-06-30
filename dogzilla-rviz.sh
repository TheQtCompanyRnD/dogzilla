#!/bin/bash
# Launch RViz on the PC to visualize the dog's namespaced SLAM.
#
# rviz2 has no positional config arg (only -d), and its TF backend subscribes to
# the absolute /tf, /tf_static -- so without these remaps it sees nothing. This
# loads the saved config and redirects TF to the robot's namespace.
#
# Usage: ./dogzilla-rviz.sh [namespace]   (default: dogzilla)
#        ./dogzilla-rviz.sh robot2
#
# This runs on the PC, not the Pi (hence top-level, not in services/).

. /opt/ros/jazzy/setup.bash

NS="${1:-dogzilla}"
CONFIG="$HOME/.rviz2/dogzilla-slam.rviz"

exec rviz2 -d "$CONFIG" \
    --ros-args \
    -r "/tf:=/$NS/tf" \
    -r "/tf_static:=/$NS/tf_static"
