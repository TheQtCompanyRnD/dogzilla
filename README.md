# meta-dogzilla

A Yocto/OpenEmbedded layer for building a headless Linux image for the
**Raspberry Pi 5** inside a **Dogzilla S2** quadruped robot, as an alternative
to the stock Ubuntu install.

The image (`ros-image-dogzilla`) provides:

- **ROS 2 Jazzy** core (`ros-core`) + Python bindings (`rclpy`)
- **Cartographer** SLAM (`cartographer-ros`)
- **Qt 6.12** (pre-release branch) runtime: `qtbase`, `qtdeclarative`,
  `qtserialport`, `qtmultimedia`, and a ported `qtgamepad`
- **Qt ROS** (`qt-ros2-bridge`), built from the `qtros` repo
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
  branch.
- The robot's own code is fetched from two upstream repos over **https**
  - [`TheQtCompanyRnD/dogzilla`](https://github.com/TheQtCompanyRnD/dogzilla) --
    `dogzillad`, `dogzilla-interfaces`, `oledd` and `dogzilla-slam` are all
    built from subdirs of this one repo, so its revision is pinned once, as
    `DOGZILLA_SRCREV` in `conf/layer.conf`.
  - [`TheQtCompanyRnD/qtros`](https://github.com/TheQtCompanyRnD/qtros) --
    Qt ROS; `SRCREV` is pinned in the recipe.

  Override `DOGZILLA_GIT_REPO` / `QTROS_GIT_REPO` to build from a fork or a
  mirror. To track a branch head while developing, set e.g.
  `SRCREV:pn-dogzillad = "${AUTOREV}"` in `local.conf`, or use
  `devtool modify`.
- While those two repos are still **private**, the fetch needs a credential,
  and `kas` replaces `$HOME` with a throwaway directory -- so a `git
  credential.helper` configured in your own `~/.gitconfig` is *not* picked up.
  Hand kas one explicitly instead. With the GitHub CLI:

  ```sh
  GIT_CREDENTIAL_HELPER="!HOME=$HOME gh auth git-credential" \
      kas build meta-dogzilla/kas/dogzilla-raspberrypi5-jazzy.yml
  ```

  (the inner `HOME=` is what lets `gh` find its own config from inside kas's
  temporary home). `NETRC_FILE=~/.netrc` works too, and plain `bitbake` outside
  kas needs none of this. All of it becomes unnecessary once the repos are
  public -- nothing here is baked into the layer.
- Log in as pi / doggo ; pi has sudo permission with no password. For key-based
  login, drop your public key in
  `recipes-core/dogzilla-users/files/home-pi/.ssh/authorized_keys` -- that path
  is gitignored, along with the rest of the pi home overlay that is per-robot
  rather than shareable (see `.gitignore`).

## License

The layer metadata (recipes, configs, this README) is MIT; see `COPYING.MIT`.
The software it builds keeps its own licenses.
