# rtabmap_launch's launch files optionally start the GUIs, so its package.xml
# <exec_depend>s on rtabmap_viz (Qt 5) and rtabmap_rviz_plugins (RViz). Neither
# can build on a headless image, which would drag the whole launch package --
# and with it the image -- down with them. Drop both: the launch files we use
# (rtabmap.launch.py with rtabmap_viz:=false rviz:=false) do not need them.
RDEPENDS:${PN}:remove = "rtabmap-viz rtabmap-rviz-plugins"
