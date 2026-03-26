import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "LfUpperLegLink"
        Model {
            id: model
            source: "meshes/LfUpperLegLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
