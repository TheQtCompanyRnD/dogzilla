# Install the dogzillad + SLAM systemd user services on the robot (run on the Pi).
#
#   make install   # copy units, scripts and SLAM config into place
#   make enable    # install + daemon-reload + enable --now (first-time setup)
#   make restart   # install + daemon-reload + restart (apply edits to running units)
#   make status    # show the default services
#   make logs      # follow both journals
#
# User services (no root): dogzillad needs the pi user in the dialout/video
# groups for serial + framebuffer; SLAM needs nothing privileged. For start at
# boot without an interactive login: loginctl enable-linger $(USER).

BIN_DIR  ?= $(HOME)/.local/bin
SLAM_DIR ?= $(HOME)/.config/dogzilla
UNIT_DIR ?= $(HOME)/.config/systemd/user

SLAM_SERVICES := dogzilla-slam-cartographer.service dogzilla-slam-toolbox.service
# Started/enabled by default; the other SLAM backend is started on demand
# (`systemctl --user start dogzilla-slam-toolbox`: Conflicts= stops the first).
SERVICES := dogzillad.service dogzilla-slam-cartographer.service

install:
	install -d $(BIN_DIR) $(SLAM_DIR) $(UNIT_DIR)
	install -m 0755 dogzillad.sh $(BIN_DIR)/
	# slam.sh self-locates, so it lives with the launch files + configs it runs.
	install -m 0755 slam.sh $(SLAM_DIR)/
	install -m 0644 cartographer.launch.py dogzilla_2d.lua \
		slam_toolbox.launch.py slam_toolbox.yaml $(SLAM_DIR)/
	install -m 0644 dogzillad.service $(SLAM_SERVICES) $(UNIT_DIR)/

reload:
	systemctl --user daemon-reload

enable: install reload
	systemctl --user enable --now $(SERVICES)

restart: install reload
	systemctl --user restart $(SERVICES)

status:
	systemctl --user status $(SERVICES) --no-pager

logs:
	journalctl --user -u dogzillad.service -u 'dogzilla-slam-*.service' -f

.PHONY: install reload enable restart status logs
