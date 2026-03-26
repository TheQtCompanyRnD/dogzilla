import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "RhUpperLegLink"
        Model {
            id: model
            source: "meshes/RhUpperLegLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
