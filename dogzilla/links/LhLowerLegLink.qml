import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "LhLowerLegLink"
        Model {
            id: model
            source: "meshes/LhLowerLegLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
