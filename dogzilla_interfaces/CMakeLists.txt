cmake_minimum_required(VERSION 3.16)
project(dogzilla_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)  # for the std_msgs/Header in StampedTelemetry

rosidl_generate_interfaces(${PROJECT_NAME}
    "msg/StampedTelemetry.msg"
    "msg/ChatMessage.msg"
    DEPENDENCIES std_msgs
)

ament_package()
