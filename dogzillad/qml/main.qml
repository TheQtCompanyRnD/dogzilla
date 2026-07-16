// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
import QtQml
import QtMultimedia
import QtUniversalInput
import Dogzilla
import Dogzilla.Telemetry
import QtRos2.Core as Ros2
import QtRos2.GeometryMsgs
import QtRos2.SensorMsgs
import QtRos2.StdMsgs
import QtRos2.Transforms

Ros2.Node {
    id: root
    nodeName: "dogzilla"
    // Namespace the node so TF lands on /dogzilla/tf_static (QtRos2 remaps
    // tf2's absolute /tf, /tf_static to follow the namespace). The other topics
    // are already written with an explicit /dogzilla/ prefix below.
    nodeNamespace: "/dogzilla"

    TwistPublisher {
        id: twp
        topic: `/${root.nodeName}/vel`
        linear.x: controller.walkSpeed
        linear.y: controller.sideStepSpeed
        angular.z: controller.steerAngle
    }

    JointStatePublisher {
        id: jsp
        topic: `/${root.nodeName}/joint_states`
        name:  [
                "lf_lower_leg_joint",
                "lf_upper_leg_joint",
                "lf_hip_joint",

                "rf_lower_leg_joint",
                "rf_upper_leg_joint",
                "rf_hip_joint",

                "lh_lower_leg_joint",
                "lh_upper_leg_joint",
                "lh_hip_joint",

                "rh_lower_leg_joint",
                "rh_upper_leg_joint",
                "rh_hip_joint",
          ]
        position: controller.jointAngles
       // could also include velocity, effort
   }

    BatteryStatePublisher {
        id: bsp
        topic: `/${root.nodeName}/battery_state`
        percentage: controller.batteryPercent / 100
        present: true
    }

    property ConsoleDashboard dash: ConsoleDashboard {
        batteryLevel: controller.batteryPercent
        tty: "/dev/tty1"
    }

    // Ros2Node apparently only allows childEntities as children:
    // if we don't declare a property, we get
    // Cannot assign object of type "QQmlConnections" to list property "childEntities"; expected "QRos2Entity"
    property Controller controller: Controller {
        serialPort: "/dev/ttyAMA0"
        baudRate: 115200
        onBatteryPercentChanged: console.log("batt", batteryPercent);
    }

    property UniversalInput univin: UniversalInput {
        /*!
            Note: to get xbox mode, hold down the mode button on the controller to
            switch to the mode where the green LED is lit. The default mode with the
            red LED is not as useful: the right joystick doesn't work, etc.
        */
        onJoyAxisEvent:
            (device, axis, value) => {
                console.log("axis", axis, value)
                switch (axis) {
                // left stick: yaw speed (steer) and walk speed
                case 0: // JoyAxis.LeftX
                    controller.steerAngle = value * -90
                    break;
                case 1: // JoyAxis.LeftY
                    controller.walkSpeed = value * -50
                    break;
                // right stick: side step (strafe) and pitch angle (look up/down)
                case 2: // JoyAxis.RightX
                    controller.pitch = value * 15
                    break;
                case 4: // should be RightY
                    controller.sideStepSpeed = (value - 0.5) * -50
                    break;
                }
            }
        onJoyButtonEvent:
            (device, button, isPressed) => {
                console.log("button", button, isPressed)
                if (!isPressed)
                    return
                switch (button) {
                case 6: // JoyButton.Start
                    const engage = !controller.motorsEngaged
                    controller.motorsEngaged = engage
                    lidar.running = engage
                    break
                case 4: // JoyButton.Back
                    controller.stop()
                    break
                }
            }
    }

    // JoySubscriber is also possible, but would usually drive a TwistPublisher

    TwistSubscriber {
        id: cmdVelSub
        topic: `/${root.nodeName}/cmd_vel`
        // TODO declarative multi-binding: either this or univin can comnmand the controller;
        // or, drive TwistPublisher from univin
        onMessageReceived: (msg) => {
            console.log("--- twist", JSON.stringify(msg), msg.linear)
            controller.sideStepSpeed = msg.linear.y
            controller.walkSpeed = msg.linear.x
            // linear.z angular.x and angular.y are documented for "aerial vehicles only"
            controller.steerAngle = msg.angular.z
        }
    }

    PoseStampedSubscriber {
        id: poseSub
        topic: `/${root.nodeName}/body_pose/command`
        onPoseChanged: {
            // pose.orientation.rpyDegrees is a ROS vector3 (degrees, double)
            // eulerAngles is a single-precision QVector3D, and would need QtQuick
            // header.frameId is available if we later want to validate/transform the frame
            controller.roll = poseSub.pose.orientation.rpyDegrees.x
            controller.pitch = poseSub.pose.orientation.rpyDegrees.y
            controller.yaw = poseSub.pose.orientation.rpyDegrees.z
        }
    }

    // Feedback from the IMU, published symmetric with body_pose/command.
    // measuredRoll/measuredPitch are tared real degrees (relative to startup, or
    // to the last controller.tareAttitude()). Yaw is published as 0: the firmware's
    // yaw is a free-running gyro integral that drifts ~14 deg/s, so the twin's
    // heading should come from odometry, not here. Referencing the measured*
    // properties is also how the Controller detects interest and starts IMU polling.
    // (Same fromEulerAngles pattern the digital twin uses on the command side.)
    PoseStampedPublisher {
        id: posePub
        topic: `/${root.nodeName}/body_pose/state`
        pose.orientation: Quaternion.fromEulerAngles(controller.measuredRoll,
                                                      controller.measuredPitch,
                                                      0)
    }

    CompressedImagePublisher {
        id: imagePublisher
        topic: `/${root.nodeName}/camera/image/compressed`
        // Best-effort: over a congested Wi-Fi link, drop frames rather than
        // retransmit/block. Stale video is useless, and reliable delivery of a
        // high-rate JPEG stream is what clogs the link. The digitwin's
        // CompressedImageSubscriber must request best-effort too, to match.
        qos: Ros2.QualityOfService.sensorData()
    }

    property CaptureSession captureSession: CaptureSession {
        property LoggingCategory cameraCategory: LoggingCategory {
            id: cameraCategory
            name: "dogzilla.camera"
            defaultLogLevel: LoggingCategory.Warning
        }

        imageCapture: ImageCapture {
            id: imageCapture
            onImageCaptured: (reqId, image) => {
                const timeMs = new Date().getTime()
                console.log(cameraCategory, "image captured", reqId, image)
                const msg = {
                    "header": {
                        "stamp": {
                            "sec": Math.trunc(timeMs / 1000),
                            "nanosec": timeMs % 1000 * 1000000
                        },
                        "frameId": reqId
                    },
                    "format": "jpeg",
                    "image": image
                };
                imagePublisher.publish(msg)
            }
        }
        camera: Camera {
            id: camera
            onErrorOccurred: (err, errorString) => console.log("camera error", errorString)
        }

        property Timer cameraTimer: Timer {
            interval: 200 // TODO increase the frequency; how to make it adaptive?
            repeat: true
            running: true // TODO only when the network is up, DDS is ok and some client is listening
            onTriggered: imageCapture.capture()
        }
    }

    LaserScanPublisher {
        id: frickenLaserPublisher
        topic: `/${root.nodeName}/sensor_msgs/msg/LaserScan`
    }

    // Static base_link -> laser_frame transform, from the URDF laser_Joint origin
    // (xyz="-0.016732 4.4164E-05 0.10335" rpy="0 0 0"). The lidar.cpp scan already
    // stamps frame_id="laser_frame", so SLAM (rf2o + slam_toolbox) can resolve it.
    // StaticTransformBroadcaster wraps tf2_ros and latches the declared transform
    // on /tf_static (transient_local), so a tf2 listener that starts later (e.g.
    // the slam node) still receives it. Declared (not sent imperatively) so it is
    // (re)published from setupConnection once the node is initialized, with no
    // race against Component.onCompleted.
    StaticTransformBroadcaster {
        transforms: [{
            "header": { "frameId": "base_link" },
            "childFrameId": "laser_frame",
            "transform": {
                "translation": { "x": -0.016732, "y": 4.4164e-05, "z": 0.10335 },
                "rotation": { "x": 0, "y": 0, "z": 0, "w": 1 }
            }
        }]
    }

    property Lidar lidar: Lidar {
        serialPort: "/dev/ttyAMA1"
        onSectorScanned: (msg) => frickenLaserPublisher.publish(msg)
    }

    // Push-to-talk speech-to-text. The digital twin toggles
    // /dogzilla/speech/listen (true = button pressed, false = released): while
    // held we silence the fan and capture the mic; on release we stop capture,
    // transcribe the utterance (whisper, on a worker thread) and publish the
    // text on /dogzilla/speech/transcript.
    property FanController fan: FanController {}
    property AudioCapture mic: AudioCapture {
        onCaptured: (pcm) => stt.transcribe(pcm)
    }
    property WhisperSpeechToText stt: WhisperSpeechToText {
        // tiny.en-q5_1 is the fast default; swap to ggml-base.en.bin for accuracy.
        modelPath: "/usr/share/whisper.cpp/models/ggml-tiny.en-q5_1.bin"
        onTranscriptReady: (text) => {
            console.log("heard:", text);
            transcriptPub.publish(text);
        }
        onErrorOccurred: (msg) => console.warn("stt:", msg)
    }

    BoolSubscriber {
        topic: `/${root.nodeName}/speech/listen`
        // std_msgs/Bool single-field collapse: the handler gets the bool directly.
        onMessageReceived: (listening) => {
            fan.quiet = listening;       // sudo dogzilla-fan quiet / auto
            mic.listening = listening;   // false edge -> captured() -> stt.transcribe()
        }
    }

    StringPublisher {
        id: transcriptPub
        topic: `/${root.nodeName}/speech/transcript`
    }

    // Readiness/status for the twin: "idle" (ready) / "listening" / "transcribing".
    // Latched (transient-local) so a twin that connects later immediately gets
    // the current state and can, e.g., enable the PTT button only when idle.
    StringPublisher {
        id: statePub
        topic: `/${root.nodeName}/speech/state`
        qos: Ros2.QualityOfService.transientLocal()
    }
    // The state itself is a declarative binding. The bridge's single-field
    // publishers (String/Bool) expose only publish() -- no bindable property --
    // so the one imperative step is publishing when the derived value changes.
    // No binding loop: this reads busy/listening and never writes them back.
    readonly property string speechState: stt.busy ? "transcribing"
                                         : mic.listening ? "listening" : "idle"
    onSpeechStateChanged: statePub.publish(speechState)

    // System telemetry (fan level, CPU temperature, CPU load) at 1 Hz, for the
    // twin's line charts -- e.g. watch PTT silence the fan and the temp/CPU
    // response. Single-field std_msgs publishers are publish()-only (no
    // bindable property), so we publish imperatively on each sample.
    property Telemetry telemetry: Telemetry {}

    // fan + cpu via our custom Dogzilla.Telemetry message. It's multi-field, so
    // it binds DECLARATIVELY -- no publish() in JS; the publisher republishes
    // when either metric changes. Generated from the dogzilla_interfaces
    // package by qtros2_generate_from_package (see CMakeLists).
    TelemetryPublisher {
        topic: `/${root.nodeName}/telemetry/system`
        fanLevel: telemetry.fanLevel
        cpuPercent: telemetry.cpuPercent
    }

    // Temperature via the standard sensor_msgs/Temperature -- also declarative.
    TemperaturePublisher {
        topic: `/${root.nodeName}/telemetry/temperature`
        temperature: telemetry.temperatureC
    }

    Component.onCompleted: {
        camera.start();
        statePub.publish(speechState);   // latch the initial "idle" so a late twin sees it
        console.log("chosen camera", camera.cameraDevice, camera.cameraFormat, "active", camera.active, "feat", camera.supportedFeatures);
    }
}
