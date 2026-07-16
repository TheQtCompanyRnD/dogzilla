# Dogzilla system telemetry (metrics with no standard ROS message).
# Temperature is published separately as sensor_msgs/Temperature.
int32 fan_level      # thermal cooling_device0 cur_state (0 = fan off)
float32 cpu_percent  # CPU load over the last sample interval, 0..100
