cmake_minimum_required(VERSION 3.16)
project(dogzilla_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)         # for the std_msgs/Header in StampedTelemetry
find_package(trajectory_msgs REQUIRED)  # for the JointTrajectory in PlayMotion

rosidl_generate_interfaces(${PROJECT_NAME}
    "msg/StampedTelemetry.msg"
    "msg/ChatMessage.msg"
    "action/Speak.action"
    "action/PlayMotion.action"
    DEPENDENCIES std_msgs trajectory_msgs
)

ament_package()
