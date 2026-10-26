// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
import QtQuick
import QtQuick3D

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

    // Shared with the main scene so the 3D stick gets the same reflections
    property Texture lightProbe
    // Node scale of the 3D stick; the camera is fixed, so the stick's size
    // on screen follows the control's size
    property real stickScale: 3.7

    // Own transparent scene, so nothing else can occlude the stick
    View3D {
        anchors.fill: parent
        camera: stickCamera
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            lightProbe: root.lightProbe
            tonemapMode: SceneEnvironment.TonemapModeHejlDawson
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            id: stickCamera
            z: 300
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-30, 30, 0)
        }

        ThumbStick {
            axes: root.axes
            scale: Qt.vector3d(root.stickScale, root.stickScale, root.stickScale)
        }
    }

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
