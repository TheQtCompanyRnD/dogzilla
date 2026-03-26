import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "LhUpperLegLink"
        Model {
            id: model
            source: "meshes/LhUpperLegLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
