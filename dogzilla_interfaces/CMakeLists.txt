cmake_minimum_required(VERSION 3.16)
project(dogzilla_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
    "msg/Telemetry.msg"
)

ament_package()
