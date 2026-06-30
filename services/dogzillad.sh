#!/bin/bash
. /opt/ros/jazzy/setup.bash
cd ~/bin
export QT_LOGGING_RULES=*.debug=false
# ; dogzilla.blah.*=true
~/bin/dogzillad -platform linuxfb
