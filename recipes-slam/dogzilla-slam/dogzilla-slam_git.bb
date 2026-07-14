SUMMARY = "Dogzilla onboard 2D SLAM (Cartographer): launch + config + on-demand user service"
DESCRIPTION = "Installs the Dogzilla 2D SLAM launch file and Cartographer .lua \
config, plus a systemd --user unit that runs them. The unit is installed but \
NOT enabled: SLAM is memory-hungry and situational, so it is started on demand \
(`systemctl --user start dogzilla-slam`, or remotely via dogzillad). It runs \
cartographer_node + cartographer_occupancy_grid_node under the /dogzilla \
namespace, consuming dogzillad's LaserScan and publishing /dogzilla/map."
HOMEPAGE = "https://git.qt.io/qt-robotics/dogzilla"

# Our own launch/config; no upstream license file to checksum (matches dogzillad).
LICENSE = "CLOSED"

# Fetch slam.launch.py + dogzilla_2d.lua from the local dogzilla checkout (same
# repo as dogzillad; DOGZILLA_SRC is set in meta-dogzilla/conf/layer.conf).
# protocol=file -> no ssh to git.qt.io, no YubiKey per fetch. AUTOREV tracks the
# last commit on main (commit edits, or use `devtool modify` for live work).
SRC_URI = "git://${DOGZILLA_SRC};protocol=file;branch=main \
           file://dogzilla-slam.service"
SRCREV = "${AUTOREV}"
PV = "1.0+git"
# S defaults to ${UNPACKDIR}/${BP} (the repo root); the SLAM files live in services/.

inherit allarch

# Runtime deps: cartographer_ros nodes + the `ros2 launch` machinery. These are
# already pulled into the image, but declaring them keeps dogzilla-slam
# self-sufficient if the base image set changes.
RDEPENDS:${PN} = " \
    cartographer-ros \
    ros2launch \
    ros2cli \
    launch-ros \
    python3-core \
"

do_install() {
    # slam.launch.py resolves dogzilla_2d.lua relative to itself, so install
    # both into the same directory.
    install -d ${D}${datadir}/dogzilla-slam
    install -m 0644 ${S}/services/slam.launch.py ${D}${datadir}/dogzilla-slam/slam.launch.py
    install -m 0644 ${S}/services/dogzilla_2d.lua ${D}${datadir}/dogzilla-slam/dogzilla_2d.lua

    # systemd --user unit, installed but NOT enabled (no default.target.wants
    # symlink) -> on-demand start, not at boot.
    install -d ${D}${systemd_user_unitdir}
    install -m 0644 ${UNPACKDIR}/dogzilla-slam.service \
        ${D}${systemd_user_unitdir}/dogzilla-slam.service
}

FILES:${PN} = " \
    ${datadir}/dogzilla-slam \
    ${systemd_user_unitdir}/dogzilla-slam.service \
"
