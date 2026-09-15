# Upstream ships this disabled (SYSTEMD_AUTO_ENABLE = "disable"); dogzilla-network-config's
# hook scripts under /etc/networkd-dispatcher/ need the daemon actually running.
SYSTEMD_AUTO_ENABLE = "enable"
