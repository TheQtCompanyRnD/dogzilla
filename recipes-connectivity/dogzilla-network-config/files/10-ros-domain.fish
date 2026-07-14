# ROS 2 domain for the robot. Must match every participant (dogzillad, any
# ros2 CLI shell here, and the laptop running the digital twin) or they can't
# discover each other -- even on the same host, a domain-0 shell won't see a
# domain-81 daemon. dogzillad gets this via its systemd unit (Environment=);
# this drop-in covers interactive fish sessions (e.g. `ros2 topic list` over
# ssh). Read by every fish shell from /etc/fish/conf.d.
set -gx ROS_DOMAIN_ID 81
