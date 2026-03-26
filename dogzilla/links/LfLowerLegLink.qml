import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "LfLowerLegLink"
        Model {
            id: model
            source: "meshes/LfLowerLegLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
