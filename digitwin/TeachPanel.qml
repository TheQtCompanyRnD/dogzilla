// Teach pendant: pose the dog by jogging per-joint sliders (model only), capture
// poses as timed waypoints, and play the sequence back on the real robot via the
// PlayMotion action. Jogging writes DogzillaControl.<joint>Angle (degrees), which
// the 3D model live-binds to; nothing goes to the robot until Play. The parent
// (PreviewScene) owns the ROS wiring and converts waypoints -> JointTrajectory.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: root

    // The Dogzilla 3D root; targetRobot.control is the DogzillaControl with the 12
    // <joint>Angle degree properties (writable) and jointInfos metadata.
    required property var targetRobot

    // Reported motor state (from the robot's motors.engaged parameter) and whether
    // a motion is currently playing; both driven by the parent.
    property bool engaged: false
    property bool playing: false
    // Live feedback from the running goal (parent sets these from action feedback).
    property int currentPoint: -1
    property real progress: 0

    signal closed()                 // ✕ button: parent hides the panel
    signal setEngaged(bool on)      // -> motors.engaged parameter
    signal play(var waypoints)      // [{ pose: {camelName: deg, ...}, duration: s }]
    signal stop()                   // cancel the in-flight goal

    padding: 8

    readonly property var control: targetRobot ? targetRobot.control : null
    readonly property var joints: control ? control.jointInfos : []
    readonly property var legLabels: ["Left front", "Right front", "Left hind", "Right hind"]

    // Write a joint (degrees) to the model; if mirroring, also write its L/R pair
    // (same value -- the URDF hip axes are already sign-flipped between sides, so
    // an equal value gives a symmetric pose; tweak by hand if a joint needs it).
    function setJoint(name, value) {
        control[name] = value;
        if (mirrorToggle.checked) {
            const m = mirroredName(name);
            if (m)
                control[m] = value;
        }
    }
    function mirroredName(name) {
        let idx = -1;
        for (let i = 0; i < joints.length; ++i)
            if (joints[i].name === name) { idx = i; break; }
        if (idx < 0)
            return "";
        // joints are grouped 3-per-leg in order LF, RF, LH, RH; pair LF<->RF and
        // LH<->RH by flipping the low bit of the leg index, same joint within it.
        const paired = ((Math.floor(idx / 3) ^ 1) * 3) + (idx % 3);
        return joints[paired] ? joints[paired].name : "";
    }

    function capturePose() {
        let pose = {};
        for (let i = 0; i < joints.length; ++i)
            pose[joints[i].name] = control[joints[i].name];
        waypointModel.append({ duration: 1.0, poseJson: JSON.stringify(pose) });
    }

    function doPlay() {
        let list = [];
        for (let i = 0; i < waypointModel.count; ++i) {
            const e = waypointModel.get(i);
            list.push({ pose: JSON.parse(e.poseJson), duration: e.duration });
        }
        root.play(list);
    }

    // Waypoints: duration = seconds to hold this pose before moving to the next.
    // pose is stored as a JSON string (robust in ListModel) of camelName -> degrees.
    ListModel { id: waypointModel }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Label { text: "Teach pendant"; font.bold: true; Layout.fillWidth: true }
            RoundButton { text: "✕"; flat: true; onClicked: root.closed() }
        }

        RowLayout {
            Switch {
                id: engageSwitch
                text: checked ? "Engaged" : "Relaxed"
                checked: root.engaged
                onToggled: root.setEngaged(checked)
            }
            Item { Layout.fillWidth: true }
            CheckBox { id: mirrorToggle; text: "Mirror L↔R" }
        }

        // Per-leg joint sliders (model only). Grouped 3 joints per leg.
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
                width: parent.width
                spacing: 4
                Repeater {
                    model: 4  // legs
                    delegate: GroupBox {
                        required property int index
                        readonly property int legStart: index * 3
                        Layout.fillWidth: true
                        title: root.legLabels[index]
                        ColumnLayout {
                            width: parent.width
                            Repeater {
                                model: 3  // joints within the leg
                                delegate: RowLayout {
                                    required property int index
                                    readonly property var info: root.joints[legStart + index]
                                    Label {
                                        text: info ? info.name.replace(/Joint(Angle)?$/, "")
                                                                .replace(/^(lf|rf|lh|rh)/, "") : ""
                                        Layout.preferredWidth: 70
                                    }
                                    Slider {
                                        id: sl
                                        enabled: !!info
                                        from: info ? info.lower : -1
                                        to: info ? info.upper : 1
                                        stepSize: 0.5
                                        Layout.fillWidth: true
                                        onMoved: if (info) root.setJoint(info.name, value)
                                        // Track the model when the user isn't dragging, so the
                                        // slider follows loaded poses / the live robot pose.
                                        Binding on value {
                                            when: info && !sl.pressed
                                            value: info ? root.control[info.name] : 0
                                            restoreMode: Binding.RestoreBindingOrValue
                                        }
                                    }
                                    Label {
                                        text: sl.value.toFixed(0) + "°"
                                        Layout.preferredWidth: 40
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Button { text: "Capture pose"; onClicked: root.capturePose() }
            Item { Layout.fillWidth: true }
            Button { text: "Clear"; enabled: waypointModel.count > 0; onClicked: waypointModel.clear() }
        }

        // The taught sequence.
        ListView {
            id: waypointView
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            clip: true
            model: waypointModel
            ScrollBar.vertical: ScrollBar {}
            delegate: RowLayout {
                required property int index
                required property real duration
                width: ListView.view.width
                Label {
                    text: (index + 1) + "."
                    color: index === root.currentPoint ? "cyan" : "white"
                    Layout.preferredWidth: 24
                }
                SpinBox {
                    from: 0; to: 60000; stepSize: 100   // milliseconds, shown as seconds
                    value: Math.round(duration * 1000)
                    editable: true
                    textFromValue: (v) => (v / 1000).toFixed(1) + "s"
                    valueFromText: (t) => Math.round(parseFloat(t) * 1000)
                    onValueModified: waypointModel.setProperty(index, "duration", value / 1000)
                }
                Item { Layout.fillWidth: true }
                RoundButton { text: "↑"; flat: true; enabled: index > 0
                    onClicked: waypointModel.move(index, index - 1, 1) }
                RoundButton { text: "↓"; flat: true; enabled: index < waypointModel.count - 1
                    onClicked: waypointModel.move(index, index + 1, 1) }
                RoundButton { text: "🗑"; flat: true; onClicked: waypointModel.remove(index) }
            }
        }

        RowLayout {
            Button {
                text: "▶ Play"
                enabled: waypointModel.count > 0 && root.engaged && !root.playing
                onClicked: root.doPlay()
            }
            Button {
                text: "⬛ Stop"
                enabled: root.playing
                onClicked: root.stop()
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: root.playing ? `playing ${root.currentPoint + 1}/${waypointModel.count}`
                    : !root.engaged ? "engage to play"
                    : `${waypointModel.count} waypoint(s)`
            }
        }
    }
}
