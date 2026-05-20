import QtQuick

Item {
    id: root
    width: 320
    height: width
    property bool polar: true

    property real margin: 20
    property real rawX: (knobHolder.x - knobHolder.halfRange.x - margin) / knobHolder.halfRange.x
    property real rawY: (knobHolder.y - knobHolder.halfRange.y - margin) / -knobHolder.halfRange.y
    property real rawR: Math.sqrt(rawX * rawX + rawY * rawY)
    property point axes:
        polar ?
            (dragHandler.active ?
                 Qt.point(rawX / Math.max(1, rawR),
                          rawY / Math.max(1, rawR)) :
                 Qt.point(0, 0)) :
            (dragHandler.active && rawR > 0 ?
                 Qt.point(rawX / Math.max(Math.abs(rawX), Math.abs(rawY)) * Math.min(rawR, 1),
                          rawY / Math.max(Math.abs(rawX), Math.abs(rawY)) * Math.min(rawR, 1)):
                 Qt.point(0, 0))

    property alias knobVisible: knob.visible

    Item {
        id: knobHolder
        // intentionally zero width & height
        property point halfRange: Qt.point(parent.width / 2 - parent.margin,
                                           parent.height / 2 - parent.margin)

        // fallback visualization in case there is no 3D knob
        Rectangle {
            id: knob
            visible: false
            width: 40; height: 40; radius: 20
            color: "red"
            anchors.centerIn: parent
        }

        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: parent.verticalCenter
        }
        states: [
            State {
                when: dragHandler.active
                AnchorChanges {
                    target: knobHolder
                    anchors.horizontalCenter: undefined
                    anchors.verticalCenter: undefined
                }
            }
        ]
        transitions: [
            Transition {
                AnchorAnimation { easing.type: Easing.OutElastic }
            }
        ]
    }

    DragHandler {
        id: dragHandler
        target: knobHolder
        xAxis {
            minimum: root.margin
            maximum: root.width - root.margin
        }
        yAxis {
            minimum: root.margin
            maximum: root.height - root.margin
        }
    }
}
