cmake_minimum_required(VERSION 3.27)
project(dogzilla LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml SerialPort UniversalInput)
find_package(WrapRos2 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(type_description_interfaces REQUIRED)

qt_standard_project_setup()

qt_add_executable(dogzillad
    src/dogzillad.cpp
)

qt_add_qml_module(dogzillad
    URI Dogzilla
    SOURCES 
        src/controller.h
        src/controller.cpp
)


target_include_directories(dogzillad PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
    ${rclcpp_INCLUDE_DIRS}
	${type_description_interfaces_INCLUDE_DIRS}
)

set_target_properties(dogzillad PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE FALSE
)

target_link_libraries(dogzillad PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::SerialPort
    Qt::UniversalInput
    ${rclcpp_LIBRARIES}
)
