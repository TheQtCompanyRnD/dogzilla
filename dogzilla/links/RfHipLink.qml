import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "RfHipLink"
        Model {
            id: model
            source: "meshes/RfHipLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
