# Dogzilla system telemetry (metrics with no standard ROS message).
# Temperature is published separately as sensor_msgs/Temperature.
# The leading std_msgs/Header makes this a "stamped" message: QtRos2 auto-fills
# header.stamp from the node clock at publish time, giving the digital twin a
# ROS timestamp to use as the x axis for time-series charts.
std_msgs/Header header
int32 fan_level      # thermal cooling_device0 cur_state (0 = fan off)
float32 cpu_percent  # CPU load over the last sample interval, 0..100
