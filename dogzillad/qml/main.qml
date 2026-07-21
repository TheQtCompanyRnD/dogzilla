// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
import QtQml
import QtMultimedia
import QtTextToSpeech
import QtUniversalInput
import Dogzilla
import Dogzilla.Interfaces
import QtRos2.Core as Ros2
import QtRos2.GeometryMsgs
import QtRos2.SensorMsgs
import QtRos2.StdMsgs
import QtRos2.Transforms

Ros2.Node {
    id: root
    nodeName: "dogzillad"   // FQN /dogzilla/dogzillad
    // Namespace the node so TF lands on /dogzilla/tf_static (QtRos2 remaps
    // tf2's absolute /tf, /tf_static to follow the namespace). All other
    // topics below derive their /dogzilla/ prefix from this too, so the
    // node name is free to describe the program rather than the robot.
    nodeNamespace: "/dogzilla"

    TwistPublisher {
        id: twp
        topic: `${root.nodeNamespace}/vel`
        linear.x: controller.walkSpeed
        linear.y: controller.sideStepSpeed
        angular.z: controller.steerAngle
    }

    JointStatePublisher {
        id: jsp
        topic: `${root.nodeNamespace}/joint_states`
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
        topic: `${root.nodeNamespace}/battery_state`
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
        topic: `${root.nodeNamespace}/cmd_vel`
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
        topic: `${root.nodeNamespace}/body_pose/command`
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
        topic: `${root.nodeNamespace}/body_pose/state`
        pose.orientation: Quaternion.fromEulerAngles(controller.measuredRoll,
                                                      controller.measuredPitch,
                                                      0)
    }

    CompressedImagePublisher {
        id: imagePublisher
        topic: `${root.nodeNamespace}/camera/image/compressed`
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
        topic: `${root.nodeNamespace}/sensor_msgs/msg/LaserScan`
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
    // transcribe the utterance (whisper, on a worker thread) and append it to
    // the conversation log on /dogzilla/speech/log as a HEARD line.
    property FanController fan: FanController {}
    property AudioCapture mic: AudioCapture {
        onCaptured: (pcm) => stt.transcribe(pcm)
    }
    property WhisperSpeechToText stt: WhisperSpeechToText {
        // tiny.en-q5_1 is the fast default; swap to ggml-base.en.bin for accuracy.
        modelPath: "/usr/share/whisper.cpp/models/ggml-tiny.en-q5_1.bin"
        onTranscriptReady: (text, confidence) => {
            console.log("heard:", text, "confidence", confidence);
            root.logChat(root.chatHeard, text, confidence);
            // Hand the utterance to the LLM; the reply comes back async on
            // ollama.onResponseReceived below. No-op until apiUrl/model are set.
            root.ollama.chat(text);
        }
        onErrorOccurred: (msg) => console.warn("stt:", msg)
    }

    // Remote Ollama LLM: whisper transcripts go in via chat(), and the reply
    // comes back on responseReceived(), which we route to speak() so the dog
    // answers out loud (and the line lands in the /speech/log chat the twin
    // shows). Set apiUrl to your Ollama host (e.g. http://192.168.x.x:11434)
    // and model to an installed model; both are still placeholders here.
    property OllamaApi ollama: OllamaApi {
        // set to the actual LLM host IP; empty means chat() is a no-op.
        apiUrl: "http://strn.local:11434"
        model: "qwen3.6:35b"
        onResponseReceived: (text) => root.speak(text)
    }

    // Text-to-speech (QtTextToSpeech via the offline flite engine -> PipeWire).
    // The default engine/voice is fine; say() is async (state goes Speaking then
    // Ready). Non-Ros2 type, so it hangs off a property like the others.
    property TextToSpeech tts: TextToSpeech {
        onErrorOccurred: (reason, msg) => console.warn("tts:", msg)
    }

    // Speak text and record it in the chat log. The single entry point for the
    // robot's voice: the /speech/say topic (below) and, later, the LLM reply
    // path both call this, so every spoken line is logged exactly once.
    function speak(text: string) {
        if (!text)
            return;
        tts.say(text);
        logChat(chatSpoken, text, 0.0);
    }

    // Append one line to the conversation log on /dogzilla/speech/log. Imperative
    // (not a binding): each call is a distinct event. header.stamp is auto-filled
    // by the publisher from the node clock, so we omit it here.
    function logChat(source: int, text: string, confidence: real) {
        speechLog.publish({ "source": source, "text": text, "confidence": confidence });
    }

    // Mirror of dogzilla_interfaces/ChatMessage's source constants: the QtRos2
    // wrapper doesn't surface ROS message constants to QML, so keep them in sync
    // with the .msg by hand (HEARD=0, SPOKEN=1, SYSTEM=2).
    readonly property int chatHeard: 0
    readonly property int chatSpoken: 1
    readonly property int chatSystem: 2

    BoolSubscriber {
        topic: `${root.nodeNamespace}/speech/listen`
        // std_msgs/Bool single-field collapse: the handler gets the bool directly.
        onMessageReceived: (listening) => {
            fan.quiet = listening;       // sudo dogzilla-fan quiet / auto
            mic.listening = listening;   // false edge -> captured() -> stt.transcribe()
        }
    }

    // The conversation, for the twin's chat log: HEARD (what the human said, with
    // STT confidence) and SPOKEN (what the dog said) lines, timestamped. Custom
    // dogzilla_interfaces/ChatMessage (not the deprecated std_msgs/String), so it
    // carries source + confidence and is Header-stamped. Default (reliable) QoS:
    // a chat log is a live stream; the twin accumulates from when it connects.
    ChatMessagePublisher {
        id: speechLog
        topic: `${root.nodeNamespace}/speech/log`
    }

    // Text for the dog to speak, published by the twin (or the external LLM
    // bridge). Lets TTS be exercised independently of STT/the LLM.
    ChatMessageSubscriber {
        topic: `${root.nodeNamespace}/speech/say`
        onMessageReceived: (msg) => root.speak(msg.text)
    }

    // Readiness/status for the twin: "idle" (ready) / "listening" / "transcribing".
    // Latched (transient-local) so a twin that connects later immediately gets
    // the current state and can, e.g., enable the PTT button only when idle.
    StringPublisher {
        id: statePub
        topic: `${root.nodeNamespace}/speech/state`
        qos: Ros2.QualityOfService.transientLocal()
        // Fully declarative: single-field publishers expose one bindable
        // property (named after the field, `data` for std_msgs/String) that
        // auto-publishes on change, and latched topics republish the stored
        // state on connect -- so the initial "idle" is latched for late
        // twins without an imperative publish. No binding loop: this reads
        // busy/listening and never writes them back.
        data: stt.busy ? "transcribing"
            : mic.listening ? "listening" : "idle"
    }

    // Audio mixer (wpctl -> PipeWire): "master" is the default sink,
    // "mic" the default source (capture gain for PTT). dogzillad shares
    // pi's user session, so no sudo. Each channel is read once at startup
    // and echoed on set; external changes (alsamixer etc.) are not tracked.
    property VolumeController volumeCtl: VolumeController {}

    // Each mixer channel is one node parameter: settable with feedback
    // (ros2 param set / RemoteParameter; out-of-range requests are rejected
    // by rclcpp from the declared bounds before we ever see them),
    // observable via /parameter_events, introspectable with
    // ros2 param describe. VolumeController is the source of truth: the
    // value binding publishes its state, and valueEdited routes external
    // sets back into it.
    Ros2.Parameter {
        name: "audio.master"
        value: volumeCtl.master
        minimum: 0.0
        maximum: 1.0
        description: "Master (default audio sink) volume"
        onValueEdited: (v) => volumeCtl.master = v
    }
    Ros2.Parameter {
        name: "audio.mic"
        value: volumeCtl.mic
        minimum: 0.0
        maximum: 1.0
        description: "Microphone (default audio source) capture gain"
        onValueEdited: (v) => volumeCtl.mic = v
    }

    // System telemetry (fan level, CPU temperature, CPU load) at 1 Hz, for the
    // twin's line charts -- e.g. watch PTT silence the fan and the temp/CPU
    // response.
    property Telemetry telemetry: Telemetry {}

    // fan + cpu via our custom dogzilla_interfaces/StampedTelemetry message. It's
    // multi-field, so it binds DECLARATIVELY -- no publish() in JS; the publisher
    // republishes when either metric changes. Because StampedTelemetry leads with a
    // std_msgs/Header, the generated publisher extends QRos2StampedPublisherBase and
    // auto-fills header.stamp from the node clock -- no manual timestamping here.
    // Generated from the dogzilla_interfaces package by qtros2_generate_from_package.
    StampedTelemetryPublisher {
        topic: `${root.nodeNamespace}/telemetry/system`
        fanLevel: telemetry.fanLevel
        cpuPercent: telemetry.cpuPercent
    }

    // Temperature via the standard sensor_msgs/Temperature -- also declarative.
    TemperaturePublisher {
        topic: `${root.nodeNamespace}/telemetry/temperature`
        temperature: telemetry.temperatureC
    }

    Component.onCompleted: {
        camera.start();
        console.log("chosen camera", camera.cameraDevice, camera.cameraFormat, "active", camera.active, "feat", camera.supportedFeatures);
    }
}
