import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "CameraLink"
        Model {
            id: model
            source: "meshes/CameraLink.mesh"
            materials: MattBlack { }
        }
    }
}
