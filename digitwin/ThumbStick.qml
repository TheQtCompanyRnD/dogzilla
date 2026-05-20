// Adapted from https://sketchfab.com/3d-models/gc-thumbstick-v001-37d3411bb68c4579b42adfb9628e9f01
// Author: paperclip (https://sketchfab.com/paperclip)
// SPDX-License-Identifier: CC-BY-4.0
import QtQuick
import QtQuick3D

Node {
    property point axes
    eulerRotation: Qt.vector3d(axes.y * -30, axes.x * 30, 0)

    Model {
        id: object_3
        source: "meshes/thumbstick.mesh"
        rotation: Qt.quaternion(0.707107, -0.707107, 0, 0)
        materials: [
            PrincipledMaterial {
                baseColor: "lightgrey"
                roughness: 0.2
                cullMode: PrincipledMaterial.NoCulling
                alphaMode: PrincipledMaterial.Opaque
            }
        ]
    }
}
