# Install the dogzillad + SLAM systemd user services on the robot (run on the Pi).
#
#   make install   # copy units, scripts and SLAM config into place
#   make enable    # install + daemon-reload + enable --now (first-time setup)
#   make restart   # install + daemon-reload + restart (apply edits to running units)
#   make status    # show both services
#   make logs      # follow both journals
#
# User services (no root): dogzillad needs the pi user in the dialout/video
# groups for serial + framebuffer; SLAM needs nothing privileged. For start at
# boot without an interactive login: loginctl enable-linger $(USER).

BIN_DIR  ?= $(HOME)/.local/bin
SLAM_DIR ?= $(HOME)/.config/dogzilla
UNIT_DIR ?= $(HOME)/.config/systemd/user

SERVICES := dogzillad.service slam.service

install:
	install -d $(BIN_DIR) $(SLAM_DIR) $(UNIT_DIR)
	install -m 0755 dogzillad.sh $(BIN_DIR)/
	# slam.sh self-locates, so it lives with the launch + lua it runs.
	install -m 0755 slam.sh $(SLAM_DIR)/
	install -m 0644 slam.launch.py dogzilla_2d.lua $(SLAM_DIR)/
	install -m 0644 $(SERVICES) $(UNIT_DIR)/

reload:
	systemctl --user daemon-reload

enable: install reload
	systemctl --user enable --now $(SERVICES)

restart: install reload
	systemctl --user restart $(SERVICES)

status:
	systemctl --user status $(SERVICES) --no-pager

logs:
	journalctl --user -u dogzillad.service -u slam.service -f

.PHONY: install reload enable restart status logs
