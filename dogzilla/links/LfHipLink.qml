import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "LfHipLink"
        Model {
            id: model
            source: "meshes/LfHipLink.mesh"
            materials: MetalFlakePaint { }
        }
    }
}
