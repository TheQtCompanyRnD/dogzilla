# meta-dogzilla

A Yocto/OpenEmbedded layer for building a headless Linux image for the
**Raspberry Pi 5** inside a **Dogzilla S2** quadruped robot, as an alternative
to the stock Ubuntu install.

The image (`ros-image-dogzilla`) provides:

- **ROS 2 Jazzy** core (`ros-core`) + Python bindings (`rclpy`)
- **Cartographer** SLAM (`cartographer-ros`)
- **Qt 6.12** (pre-release branch) runtime: `qtbase`, `qtdeclarative`,
  `qtserialport`, `qtmultimedia`, and a ported `qtgamepad`
- The **Qt ROS 2 bridge** (`qt-ros2-bridge`), built from a local checkout
- **NetworkManager** (ethernet/wifi handoff) + **avahi** (mDNS)
- On-target development tools: gcc/g++/make, headers and `-dev` packages
- No window system — the device only runs daemons

## Building

```sh
kas build meta-dogzilla/kas/dogzilla-raspberrypi5-jazzy.yml
```

## Notes

- **qtgamepad** is not part of meta-qt6 (it was dropped as a standard Qt 6
  module). `recipes-qt/qt6/qtgamepad_git.bb` ports it against the Qt 6.12
  branch. It is unmaintained upstream and may need patching; see the recipe.
- The **Qt ROS 2 bridge** is fetched from your local working copy
  (`git.qt.io:qt-robotics/qt-ros2-bridge`) over the filesystem to avoid a
  YubiKey touch on every fetch. Set `QT_ROS2_BRIDGE_SRC` if your checkout is
  not at `~/dev/qt-robotics/qt-ros2-bridge`.
