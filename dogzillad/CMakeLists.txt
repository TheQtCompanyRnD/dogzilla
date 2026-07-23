cmake_minimum_required(VERSION 3.27)
project(dogzilla LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Network SerialPort UniversalInput Multimedia Concurrent)
find_package(Qt6Ros2Core REQUIRED)
find_package(WrapRos2 REQUIRED)
find_package(rclcpp REQUIRED)
find_package(type_description_interfaces REQUIRED)
find_package(dogzilla_interfaces REQUIRED)
# Locate rosidl_generator_qtros2 (its cmake sits alongside Qt6Ros2Core's, not on
# the default path). This sets rosidl_generator_qtros2_DIR so the generator finds
# its helper scripts (dependency_analyzer.py etc.); qt_ros2_configure_target()
# below additionally registers the prebuilt wrappers. Both are needed.
find_package(rosidl_generator_qtros2 REQUIRED PATHS "${Qt6Ros2Core_DIR}" NO_DEFAULT_PATH)
# libpulse for VolumeController (talks to pipewire-pulse; the libpulse0 client
# stack is already in the image). Located via pkg-config, which respects the
# Yocto sysroot -- unlike whisper's find_library, see the note below.
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)
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
        src/volumecontroller.h
        src/volumecontroller.cpp
        src/audiocapture.h
        src/audiocapture.cpp
        src/whisperspeechtotext.h
        src/whisperspeechtotext.cpp
        src/telemetry.h
        src/telemetry.cpp
        src/ollamaapi.h
        src/ollamaapi.cpp
        src/ollamaresponse.h
        src/ollamaresponse.cpp
        src/motionstore.h
        src/motionstore.cpp
)

# Register the prebuilt QtRos2 message wrappers (std_msgs etc.) in the generator's
# dependency registry and pull in qtros2_generate_from_package(). Required so the
# generator can resolve StampedTelemetry's std_msgs/Header dependency to the
# installed Qt6::QtRos2StandardMessages wrapper (the downstream find_package
# fallback looks for the ament-style name and misses the Qt6-style package).
qt_ros2_configure_target(dogzillad CAPABILITIES PUBLISHER SUBSCRIBER)

# Generate the QtRos2 QML wrappers (import Dogzilla.Interfaces) for our custom
# dogzilla_interfaces package (StampedTelemetry, ChatMessage), via the bridge's
# downstream macro. The whole package maps to one QML module, so the URI is the
# generic package name rather than any single message.
# TARGET dogzilla_interfaces_msgs -> CMake target DogzillaInterfacesMsgs.
qtros2_generate_from_package(
    SOURCE_PACKAGE dogzilla_interfaces
    TARGET dogzilla_interfaces_msgs
    QML_MODULE_URI Dogzilla.Interfaces
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
    Qt6::Network
    Qt6::SerialPort
    Qt::UniversalInput
    Qt6::Multimedia
    Qt6::Concurrent
    Qt6::Ros2Core
    ${rclcpp_LIBRARIES}
    whisper
    ggml
    ggml-base
    PkgConfig::PULSE
    DogzillaInterfacesMsgs
)
