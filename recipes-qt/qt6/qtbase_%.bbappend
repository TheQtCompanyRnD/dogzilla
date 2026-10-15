# QtDBus: dogzillad's ServiceController talks to the systemd user manager
# (org.freedesktop.systemd1) over the session bus. Usually already on by
# default in meta-qt6; stated here so it can't silently drop out.
PACKAGECONFIG:append = " dbus"
