import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "RhLowerLegLink"
        Model {
            id: model
            source: "meshes/RhLowerLegLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
