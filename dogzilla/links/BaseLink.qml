import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "BaseLink"
        Model {
            id: model
            source: "meshes/BaseLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
