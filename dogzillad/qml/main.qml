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
            root.ollama.chat(text)
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
        onResponseReceived: (text) => {
            // If a twin Speak(use_llm) goal is waiting on this reply, speak it
            // as part of that goal so the twin's stop button can interrupt it;
            // otherwise this is the autonomous STT->LLM path.
            if (root.activeSpeak)
                root.speakForGoal(text);
            else
                root.speak(text);
        }
    }

    // Text-to-speech (QtTextToSpeech via the offline flite engine -> PipeWire).
    // The default engine/voice is fine; say() is async (state goes Speaking then
    // Ready). Non-Ros2 type, so it hangs off a property like the others.
    property TextToSpeech tts: TextToSpeech {
        onErrorOccurred: (reason, msg) => console.warn("tts:", msg)
        // TTS returned to Ready: succeed the goal whose speech was actually
        // playing. We key off speakingGoal, not activeSpeak, because Ready is
        // ALSO the state after stop() -- so a stop() from a supersede/cancel
        // would otherwise land here and falsely succeed the *next* goal (which
        // is only "thinking", not speaking). Clearing speakingGoal before every
        // stop() makes those stray Ready transitions no-ops.
        onStateChanged: {
            if (state === TextToSpeech.Ready && root.speakingGoal) {
                root.speakingGoal.succeed({ spokenText: root.activeSpokenText, completed: true });
                if (root.activeSpeak === root.speakingGoal)
                    root.activeSpeak = null;
                root.speakingGoal = null;
            }
        }
    }

    // Speak text and record it in the chat log. The single entry point for the
    // robot's voice: the Speak action and the autonomous STT->LLM path both call
    // this, so every spoken line is logged exactly once. The chat log keeps the
    // original text (the twin's ChatView renders markdown), but TTS gets a
    // stripped copy -- flite/QTextToSpeech have no emphasis/SSML support, so an
    // LLM's "**bold**" would otherwise be read aloud as "asterisk asterisk".
    function speak(text: string) {
        if (!text)
            return;
        tts.say(stripForSpeech(text));
        logChat(chatSpoken, text, 0.0);
    }

    // Drop markdown so it isn't spoken literally. QtCore-only (plain JS RegExp):
    // dogzillad is a headless QCoreApplication, so QTextDocument (QtGui) is out.
    // Handles the inline markup an LLM typically emits; the log keeps the raw
    // text for rich display in the twin.
    function stripForSpeech(md: string): string {
        return md
            .replace(/```[\s\S]*?```/g, " ")         // fenced code blocks
            .replace(/`([^`]+)`/g, "$1")             // inline code
            .replace(/!\[[^\]]*\]\([^)]*\)/g, "")    // images
            .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1") // links -> link text
            .replace(/(\*\*|__)(.*?)\1/g, "$2")      // bold
            .replace(/(\*|_)(.*?)\1/g, "$2")         // italic
            .replace(/~~(.*?)~~/g, "$2")             // strikethrough
            .replace(/^\s{0,3}#{1,6}\s+/gm, "")      // headings
            .replace(/^\s*>+\s?/gm, "")              // blockquotes
            .replace(/^\s*[-*+]\s+/gm, "")           // bullet lists
            .replace(/^\s*\d+\.\s+/gm, "")           // numbered lists
            .replace(/\s+/g, " ")                    // collapse whitespace
            .trim();
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

    // ---- Speak action (the twin's "Speak"/"Send" buttons + "stop") --------
    // The currently-executing Speak goal handle, or null. TTS and LLM
    // completion are tied to it so the twin's stop button (an action cancel)
    // can interrupt speech and report the goal as canceled.
    property var activeSpeak: null
    property string activeSpokenText: ""
    // The goal whose TTS is actually playing right now (set when say() starts,
    // cleared before any stop() and on completion). Distinct from activeSpeak:
    // an active goal can be "thinking" (awaiting the LLM) and not yet speaking.
    property var speakingGoal: null

    // Start executing a Speak goal from the twin. use_llm=false speaks the text
    // verbatim; use_llm=true asks the LLM first and speaks the reply. Either way
    // the goal stays active until TTS finishes (succeed) or the twin cancels it.
    function beginSpeak(handle, goal) {
        // Supersede any goal still in flight: one voice, one goal at a time.
        if (activeSpeak && activeSpeak !== handle) {
            speakingGoal = null;   // before stop(): don't let its Ready succeed anything
            tts.stop();
            activeSpeak.abort({ spokenText: activeSpokenText, completed: false });
        }
        activeSpeak = handle;
        activeSpokenText = "";
        // The stop button cancels the goal; interrupt TTS and finish canceled.
        // NB: on the goal handle, cancelRequested is a bool Q_PROPERTY *and* a
        // same-named signal; in QML the property shadows the signal, so
        // `handle.cancelRequested` is the bool (not connectable). Connect to the
        // property's notifier instead and read the flag (it only latches true).
        handle.cancelRequestedChanged.connect(() => {
            if (!handle.cancelRequested || root.activeSpeak !== handle)
                return;
            root.speakingGoal = null;   // before stop(): suppress the stray Ready
            tts.stop();
            handle.canceled({ spokenText: root.activeSpokenText, completed: false });
            root.activeSpeak = null;
        });
        if (goal.useLlm) {
            handle.publishFeedback({ state: "thinking", spokenSoFar: "" });
            ollama.chat(goal.text);
        } else {
            speakForGoal(goal.text);
        }
    }

    // Speak text as part of the active goal: publish "speaking" feedback, then
    // hand off to speak() (TTS + chat log). Empty text completes immediately.
    function speakForGoal(text: string) {
        activeSpokenText = text;
        if (!activeSpeak)
            return;
        if (!text) {
            activeSpeak.succeed({ spokenText: "", completed: true });
            activeSpeak = null;
            return;
        }
        activeSpeak.publishFeedback({ state: "speaking", spokenSoFar: text });
        speakingGoal = activeSpeak;   // this goal's speech is starting; Ready now means "done"
        speak(text);
    }

    // ---- PlayMotion action (the twin's teach-pendant playback + "stop") ----
    // Plays a taught trajectory_msgs/JointTrajectory: drive to each waypoint's
    // joint angles in turn, dwelling per the point's time_from_start. We send one
    // set of per-servo targets per waypoint and let the firmware slew (MotorSpeed),
    // rather than software-interpolating every tick -- at 115200 baud, 12 servo
    // writes per frame per tick would swamp the link. Modeled on the Speak action.
    property var activeMotion: null            // the in-flight PlayMotion handle, or null
    property var motionPositions: []           // per-waypoint 12-elem radian arrays (canonical order)
    property var motionDurations: []           // ms to dwell after each waypoint
    property int motionIndex: -1

    // Canonical joint order = the JointStatePublisher name list above = the order
    // Controller::setJointAngles expects. Incoming trajectories are reordered to it.
    readonly property var motionJointOrder: [
        "lf_lower_leg_joint", "lf_upper_leg_joint", "lf_hip_joint",
        "rf_lower_leg_joint", "rf_upper_leg_joint", "rf_hip_joint",
        "lh_lower_leg_joint", "lh_upper_leg_joint", "lh_hip_joint",
        "rh_lower_leg_joint", "rh_upper_leg_joint", "rh_hip_joint",
    ]

    // Held in a property, not a bare child: the root Ros2.Node only accepts
    // QRos2NodeChild in its default childEntities list (same reason controller/tts
    // are properties above), so a bare Timer aborts QML load.
    property Timer motionTimer: Timer {
        repeat: false
        onTriggered: root.advanceMotion()
    }

    function beginMotion(handle, goal) {
        // One motion at a time: abort any in flight.
        if (activeMotion && activeMotion !== handle) {
            motionTimer.stop();
            activeMotion.abort({ completed: false });
            activeMotion = null;
        }
        if (!controller.motorsEngaged) {
            console.warn("PlayMotion: motors disengaged; aborting");
            handle.abort({ completed: false });
            return;
        }
        const traj = goal.trajectory;
        const pts = traj ? traj.points : [];
        if (!pts || pts.length === 0) {
            console.warn("PlayMotion: empty trajectory; aborting");
            handle.abort({ completed: false });
            return;
        }
        // Reorder each waypoint's positions to canonical joint order (requires all
        // 12 joints); compute per-segment dwell from cumulative time_from_start.
        let positions = [];
        let times = [];
        for (let k = 0; k < pts.length; ++k) {
            const canon = root.toCanonicalPositions(traj.jointNames, pts[k].positions);
            if (!canon) {
                console.warn("PlayMotion: waypoint", k, "is not a full 12-joint pose; aborting");
                handle.abort({ completed: false });
                return;
            }
            positions.push(canon);
            const t = pts[k].timeFromStart;
            times.push(t.sec * 1000 + t.nanosec / 1e6);
        }
        let durations = [];
        for (let k = 0; k < positions.length; ++k)
            durations.push(k < positions.length - 1 ? Math.max(0, times[k + 1] - times[k]) : 300);

        root.motionPositions = positions;
        root.motionDurations = durations;
        activeMotion = handle;
        // Stop button = action cancel (same property-shadows-signal caveat as Speak).
        handle.cancelRequestedChanged.connect(() => {
            if (!handle.cancelRequested || root.activeMotion !== handle)
                return;
            motionTimer.stop();
            handle.canceled({ completed: false });   // hold the current pose
            root.activeMotion = null;
        });

        controller.setMotorSpeed(80);   // moderate slew; crouch()/sit-style poses use ~30-80
        root.motionIndex = -1;
        root.advanceMotion();
    }

    // Drive to the next waypoint (or succeed after the last one's dwell).
    function advanceMotion() {
        if (!activeMotion)
            return;
        root.motionIndex++;
        const i = root.motionIndex;
        if (i >= root.motionPositions.length) {
            activeMotion.succeed({ completed: true });
            activeMotion = null;
            return;
        }
        controller.setJointAngles(root.motionPositions[i]);
        activeMotion.publishFeedback({ currentPoint: i, progress: (i + 1) / root.motionPositions.length });
        motionTimer.interval = root.motionDurations[i];
        motionTimer.start();
    }

    // Reorder a waypoint's positions into canonical joint order. Returns null if it
    // isn't a full 12-joint pose (a name is missing, or the wrong length) -- v1
    // plays whole-body poses only. Empty joint_names => assume already canonical.
    function toCanonicalPositions(names, positions) {
        if (!positions || positions.length !== 12)
            return null;
        if (!names || names.length === 0)
            return positions;
        let out = new Array(12);
        for (let c = 0; c < 12; ++c) {
            const idx = names.indexOf(root.motionJointOrder[c]);
            if (idx < 0)
                return null;
            out[c] = positions[idx];
        }
        return out;
    }

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

    // The twin's "Speak" / "Send" buttons: one cancellable Speak goal. The
    // goal's use_llm flag selects verbatim TTS ("Speak") vs LLM-then-speak
    // ("Send"); the twin's stop button cancels the active goal, which stops
    // TTS. Replaces the older fire-and-forget /speech/say, /speech/respond and
    // /speech/stop topics.
    SpeakActionServer {
        topic: `${root.nodeNamespace}/speech/speak`
        onGoalReceived: (goal, handle) => root.beginSpeak(handle, goal)
    }

    // Teach-pendant playback: the twin sends a taught JointTrajectory here; the
    // robot walks its waypoints (see beginMotion). Cancel = the twin's stop button.
    PlayMotionActionServer {
        topic: `${root.nodeNamespace}/motion/play`
        onGoalReceived: (goal, handle) => root.beginMotion(handle, goal)
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

    // Engage (load/stand) or disengage (unload/relax) the leg servos from the
    // twin, without needing the joystick -- and the teach pendant needs the dog
    // engaged before playing a motion. A bool node parameter so it's both
    // settable and observable-with-feedback like audio.*: the value binding
    // republishes when the joystick Start toggles motorsEngaged (so the twin
    // stays in sync with the actual state), and valueEdited routes external sets
    // into the controller. Controller is the source of truth.
    Ros2.Parameter {
        name: "motors.engaged"
        value: controller.motorsEngaged
        description: "Leg servos engaged (loaded/standing) vs relaxed"
        onValueEdited: (v) => controller.motorsEngaged = v
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
