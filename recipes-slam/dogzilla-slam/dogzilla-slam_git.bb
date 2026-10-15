SUMMARY = "Dogzilla onboard 2D SLAM (Cartographer or slam_toolbox): launch + config + on-demand user services"
DESCRIPTION = "Installs the Dogzilla 2D SLAM launch files and configs (Cartographer \
.lua; slam_toolbox .yaml), plus one systemd --user unit per backend \
(dogzilla-slam-cartographer, dogzilla-slam-toolbox). The units Conflict= with each \
other and are installed but NOT enabled: SLAM is memory-hungry and situational, so \
it is started on demand (`systemctl --user start dogzilla-slam-<backend>`, or \
remotely via dogzillad). Either backend consumes dogzillad's LaserScan and \
publishes /dogzilla/map plus map->odom."
HOMEPAGE = "https://github.com/TheQtCompanyRnD/dogzilla"

# BSD-3-Clause, per the LICENSE at the root of the dogzilla repo.
LICENSE = "BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=e7df2cb5b828712253329843cc2d34ca"

# slam.launch.py + dogzilla_2d.lua come from the same repo as dogzillad.
# Fetched over https from the upstream dogzilla repo (DOGZILLA_GIT_REPO and
# DOGZILLA_SRCREV are set in meta-dogzilla/conf/layer.conf)
# DOGZILLA_SRCREV tracks the branch head (AUTOREV) unless pinned;
# `devtool modify dogzilla-slam` for live working-tree iteration.
SRC_URI = "git://${DOGZILLA_GIT_REPO};protocol=https;branch=main \
           file://dogzilla-slam-cartographer.service \
           file://dogzilla-slam-toolbox.service"
SRCREV = "${DOGZILLA_SRCREV}"
PV = "1.0+git"
# S defaults to ${UNPACKDIR}/${BP} (the repo root); the SLAM files live in services/.

inherit allarch

# Runtime deps: the SLAM nodes + the `ros2 launch` machinery. These are
# already pulled into the image, but declaring them keeps dogzilla-slam
# self-sufficient if the base image set changes.
RDEPENDS:${PN} = " \
    cartographer-ros \
    slam-toolbox \
    tf2-ros \
    ros2launch \
    ros2cli \
    launch-ros \
    python3-core \
"

do_install() {
    # The launch files resolve their config (dogzilla_2d.lua, slam_toolbox.yaml)
    # relative to themselves, so install everything into the same directory.
    install -d ${D}${datadir}/dogzilla-slam
    install -m 0644 ${S}/services/cartographer.launch.py ${D}${datadir}/dogzilla-slam/
    install -m 0644 ${S}/services/dogzilla_2d.lua ${D}${datadir}/dogzilla-slam/
    install -m 0644 ${S}/services/slam_toolbox.launch.py ${D}${datadir}/dogzilla-slam/
    install -m 0644 ${S}/services/slam_toolbox.yaml ${D}${datadir}/dogzilla-slam/

    # systemd --user units, installed but NOT enabled (no default.target.wants
    # symlink) -> on-demand start, not at boot.
    install -d ${D}${systemd_user_unitdir}
    install -m 0644 ${UNPACKDIR}/dogzilla-slam-cartographer.service \
        ${D}${systemd_user_unitdir}/dogzilla-slam-cartographer.service
    install -m 0644 ${UNPACKDIR}/dogzilla-slam-toolbox.service \
        ${D}${systemd_user_unitdir}/dogzilla-slam-toolbox.service
}

FILES:${PN} = " \
    ${datadir}/dogzilla-slam \
    ${systemd_user_unitdir}/dogzilla-slam-cartographer.service \
    ${systemd_user_unitdir}/dogzilla-slam-toolbox.service \
"
