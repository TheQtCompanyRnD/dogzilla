cmake_minimum_required(VERSION 3.27)
project(dogzilla LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(geometry_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(turtlesim_msgs REQUIRED)

qt_standard_project_setup()

qt_add_executable(dogzillad
    src/dogzillad.cpp
    src/turtle.cpp include/turtlesim/turtle.hpp
    src/turtle_frame.cpp include/turtlesim/turtle_frame.hpp
)

target_include_directories(dogzillad PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
    ${geometry_msgs_INCLUDE_DIRS}
    ${rclcpp_INCLUDE_DIRS}
    ${rclcpp_action_INCLUDE_DIRS}
    ${std_msgs_INCLUDE_DIRS}
    ${std_srvs_INCLUDE_DIRS}
    ${turtlesim_msgs_INCLUDE_DIRS}
)

#~ message("check include ${rosidl_typesupport_interface_INCLUDE_DIRS}")
#~ message("check libs ${geometry_msgs_LIBRARIES}")

set_target_properties(dogzillad PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE FALSE
)

target_link_libraries(dogzillad PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${geometry_msgs_LIBRARIES}
    ${rclcpp_LIBRARIES}
    ${rclcpp_action_LIBRARIES}
    ${std_msgs_LIBRARIES}
    ${std_srvs_LIBRARIES}
    ${turtlesim_msgs_LIBRARIES}
)
