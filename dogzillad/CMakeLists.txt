cmake_minimum_required(VERSION 3.27)
project(dogzilla LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Qml SerialPort UniversalInput Multimedia Concurrent)
find_package(Qt6Ros2Core REQUIRED)
find_package(WrapRos2 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(type_description_interfaces REQUIRED)
find_package(dogzilla_interfaces REQUIRED)
# qtros2_generate_from_package() (used below) lives in rosidl_generator_qtros2,
# whose cmake config is installed alongside Qt6Ros2Core's (see QtRos2Macros.cmake)
# rather than on the default search path -- point find_package there.
find_package(rosidl_generator_qtros2 REQUIRED PATHS "${Qt6Ros2Core_DIR}" NO_DEFAULT_PATH)
# NB: we deliberately do NOT find_package(whisper). Its whisper-config.cmake
# resolves libwhisper with find_library(... NO_CMAKE_FIND_ROOT_PATH), which in a
# Yocto cross build defeats --sysroot and links the x86_64 HOST /usr/lib copy
# ("file in wrong format"). Instead we link the sysroot libs by plain -l name
# below (headers are on the default sysroot include path).

qt_standard_project_setup()

qt_add_executable(dogzillad
    src/dogzillad.cpp
)

qt_add_qml_module(dogzillad
    URI Dogzilla
    SOURCES 
        src/controller.h
        src/controller.cpp
        src/consoledashboard.h
        src/consoledashboard.cpp
        src/lidar.h
        src/lidar.cpp
        src/fancontroller.h
        src/fancontroller.cpp
        src/audiocapture.h
        src/audiocapture.cpp
        src/whisperspeechtotext.h
        src/whisperspeechtotext.cpp
        src/telemetry.h
        src/telemetry.cpp
)

# Generate the QtRos2 QML wrapper (import Dogzilla.Telemetry) for our custom
# dogzilla_interfaces/Telemetry message, via the bridge's downstream macro
# (installed with rosidl_generator_qtros2). No upstream-bridge changes needed.
# TARGET dogzilla_telemetry_msgs -> CMake target DogzillaTelemetryMsgs.
qtros2_generate_from_package(
    SOURCE_PACKAGE dogzilla_interfaces
    TARGET dogzilla_telemetry_msgs
    QML_MODULE_URI Dogzilla.Telemetry
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
    Qt6::Qml
    Qt6::SerialPort
    Qt::UniversalInput
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Ros2Core
    ${rclcpp_LIBRARIES}
    whisper
    ggml
    ggml-base
    DogzillaTelemetryMsgs
)
