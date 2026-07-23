// Teach pendant: pose the dog by jogging per-joint sliders (model only), capture
// poses as timed waypoints, and play the sequence back on the real robot via the
// PlayMotion action. Jogging writes DogzillaControl.<joint>Angle (degrees), which
// the 3D model live-binds to; nothing goes to the robot until Play. The parent
// (PreviewScene) owns the ROS wiring and converts waypoints -> JointTrajectory.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dogzilla

ColumnLayout {
    id: root
    spacing: 6

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

    // The waypoint currently being edited (-1 = none/free posing). Selecting a row
    // loads its pose into the model; slider edits then write back to that row.
    property int selectedIndex: -1

    signal closed()                 // ✕ button: parent hides the panel
    signal play(var waypoints)      // [{ pose: {camelName: deg, ...}, duration: s }]
    signal stop()                   // cancel the in-flight goal

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
        syncSelected();   // if a waypoint row is selected, keep it live-edited
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

    function currentPose() {
        let pose = {};
        for (let i = 0; i < joints.length; ++i)
            pose[joints[i].name] = control[joints[i].name];
        return pose;
    }

    function capturePose() {
        waypointModel.append({ duration: 0.1, poseJson: JSON.stringify(currentPose()) });
        root.selectedIndex = waypointModel.count - 1;   // select the new row for editing
    }

    // Load a waypoint's pose into the model (moves the sliders + 3D preview).
    function loadPose(i) {
        if (i < 0 || i >= waypointModel.count)
            return;
        const pose = JSON.parse(waypointModel.get(i).poseJson);
        for (let j = 0; j < joints.length; ++j) {
            const n = joints[j].name;
            if (pose[n] !== undefined)
                control[n] = pose[n];
        }
    }

    // Click a row to edit it (preview + live edits); click it again to deselect.
    function selectRow(i) {
        if (selectedIndex === i) {
            selectedIndex = -1;   // deselect -> free posing (edits no longer write back)
            return;
        }
        selectedIndex = i;
        loadPose(i);
    }

    // Write the current model pose back into the selected waypoint (live editing).
    function syncSelected() {
        if (selectedIndex < 0 || selectedIndex >= waypointModel.count)
            return;
        waypointModel.setProperty(selectedIndex, "poseJson", JSON.stringify(currentPose()));
    }

    // The current sequence as a plain waypoint array [{ pose, duration }].
    function toList() {
        let list = [];
        for (let i = 0; i < waypointModel.count; ++i) {
            const e = waypointModel.get(i);
            list.push({ pose: JSON.parse(e.poseJson), duration: e.duration });
        }
        return list;
    }

    function doPlay() {
        root.play(toList());
    }

    // Persist / restore named motions (JSON on disk via MotionLibrary).
    function saveMotion(name) {
        if (!name || waypointModel.count === 0)
            return;
        library.save(name, JSON.stringify(toList()));
    }
    function loadMotion(name) {
        const s = library.load(name);
        if (!s)
            return;
        const list = JSON.parse(s);
        root.selectedIndex = -1;
        waypointModel.clear();
        for (const wp of list)
            waypointModel.append({ duration: wp.duration, poseJson: JSON.stringify(wp.pose) });
    }

    // Waypoints: duration = seconds to hold this pose before moving to the next.
    // pose is stored as a JSON string (robust in ListModel) of camelName -> degrees.
    ListModel { id: waypointModel }
    MotionLibrary { id: library }

    // Per-leg joint sliders (model only). Grouped 3 joints per leg.
    GridLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        columns: 2
        columnSpacing: 4
        rowSpacing: 4
        palette.windowText: "white"
        Repeater {
            model: 4  // legs
            delegate: GroupBox {
                id: sliderGB
                required property int index
                readonly property int legStart: index * 3
                Layout.fillWidth: true
                title: root.legLabels[index]
                ColumnLayout {
                    width: parent.width
                    spacing: 2
                    Repeater {
                        model: 3  // joints within the leg
                        delegate: RowLayout {
                            required property int index
                            readonly property var info: root.joints[legStart + index]
                            Label {
                                text: info ? info.name.replace(/Joint(Angle)?$/, "")
                                             .replace("Leg", "")
                                             .replace(/^(lf|rf|lh|rh)/, "") : ""
                                Layout.preferredWidth: 40
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
                                Layout.preferredWidth: 30
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        Action { id: captureAction; shortcut: StandardKey.Copy; text: qsTr("&Capture pose"); onTriggered: root.capturePose() }
        Button { action: captureAction }
        Item { Layout.fillWidth: true }
        CheckBox { id: mirrorToggle; checked: true; text: "Mirror L↔R"; palette.windowText: "white" }
        Button { text: "Clear"; enabled: waypointModel.count > 0
            onClicked: { root.selectedIndex = -1; waypointModel.clear() } }
    }

    // The taught sequence.
    ListView {
        id: waypointView
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 140
        clip: true
        model: waypointModel
        spacing: 2
        move: Transition {
            NumberAnimation { properties: "x,y"; duration: 100 }
        }
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            id: wpDelegate
            required property int index
            required property real duration
            width: ListView.view.width
            implicitHeight: wpRow.implicitHeight
            // Highlight the selected (editable) row; the playing point is cyan.
            color: index === root.selectedIndex ? "steelblue" : "#44888888"
            border.color: "transparent"
            // Click to select/edit this waypoint, click again to deselect.
            TapHandler { onTapped: root.selectRow(index) }
            RowLayout {
                id: wpRow
                anchors.fill: parent
                anchors.leftMargin: 2
                Label {
                    text: (index + 1) + "."
                    color: index === root.currentPoint ? "cyan" : "white"
                    Layout.preferredWidth: 24
                }
                SpinBox {
                    from: 0; to: 60000; stepSize: 100   // milliseconds, shown as seconds
                    value: Math.round(wpDelegate.duration * 1000)
                    editable: true
                    textFromValue: (v) => (v / 1000).toFixed(1) + "s"
                    valueFromText: (t) => Math.round(parseFloat(t) * 1000)
                    onValueModified: waypointModel.setProperty(index, "duration", value / 1000)
                }
                Item { Layout.fillWidth: true }
                // Reordering/removing shifts indices, so drop the selection to avoid
                // editing the wrong row afterward.
                RoundButton { text: "↑"; flat: true; enabled: index > 0
                    onClicked: { root.selectedIndex = -1; waypointModel.move(index, index - 1, 1) } }
                RoundButton { text: "↓"; flat: true; enabled: index < waypointModel.count - 1
                    onClicked: { root.selectedIndex = -1; waypointModel.move(index, index + 1, 1) } }
                RoundButton { text: "🗑"; flat: true; palette.buttonText: "white"
                    onClicked: { root.selectedIndex = -1; waypointModel.remove(index) } }
            }
        }
    }

    // Named-motion library: type a name and Save, or pick a saved one to Load/Delete.
    RowLayout {
        ComboBox {
            id: libraryCombo
            Layout.fillWidth: true
            editable: true
            model: library.names
            // Type a new name to Save, or pick an existing motion to Load/Delete.
        }
        Button {
            text: "Save"
            enabled: libraryCombo.editText.length > 0 && waypointModel.count > 0
            onClicked: root.saveMotion(libraryCombo.editText)
        }
        Button {
            text: "Load"
            enabled: library.names.indexOf(libraryCombo.editText) >= 0
            onClicked: root.loadMotion(libraryCombo.editText)
        }
        Button {
            text: "🗑"
            enabled: library.names.indexOf(libraryCombo.editText) >= 0
            onClicked: library.remove(libraryCombo.editText)
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
