import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "RhHipLink"
        Model {
            id: model
            source: "meshes/RhHipLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
