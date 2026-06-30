#!/bin/bash
. /opt/ros/jazzy/setup.bash
# Resolve the launch + dogzilla_2d.lua relative to this script, so they only
# need to be deployed together (e.g. all of slam.sh, slam.launch.py and
# dogzilla_2d.lua in ~/bin).
DIR="$(dirname "$(readlink -f "$0")")"
exec ros2 launch "$DIR/slam.launch.py"
