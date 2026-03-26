import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "LhHipLink"
        Model {
            id: model
            source: "meshes/LhHipLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
