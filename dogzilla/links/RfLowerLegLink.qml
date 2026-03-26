import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "RfLowerLegLink"
        Model {
            id: model
            source: "meshes/RfLowerLegLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
