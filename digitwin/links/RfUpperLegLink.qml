import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "RfUpperLegLink"
        Model {
            id: model
            source: "meshes/RfUpperLegLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
