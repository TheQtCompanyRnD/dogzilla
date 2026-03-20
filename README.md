# Dogzilla daemon

An attempt to replace `DOGZILLA/app_dogzilla/app_dogzilla.py` on the 
[Dogzilla S2](https://category.yahboom.net/products/dogzilla-s1?variant=46390953640252)
with a Qt-based daemon, using ROS as one means of remote control / monitoring,
in addition to the game controller.

So far this is meant to be checked out under a ROS workspace, e.g. 
`~/ros2_ws/src/dogzilla`

```
$ source /opt/ros/kilted/setup.bash
$ cd ~/ros2_ws
$ colcon _build_ --packages-select dogzilla --cmake-args -DCMAKE_BUILD_TYPE=Debug
$ cd build/dogzilla
$ ./dogzillad -platform linuxfb
```
The linker arguments are enormous, so linking takes a couple of minutes.

After running colcon once, it's ok to run make in the build dir.

For now, it's a dog that can be partially remote-controlled via the game
controller, and plays an invisible turtle on ~~TV~~ the network.

