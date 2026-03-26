import QtQuick
import QtQuick3D
import "../materials"

Node {
    id: node

    Node {
        objectName: "LaserLink"
        Model {
            id: model
            source: "meshes/LaserLink.mesh"
            materials: ShinyBlackPlastic { }
        }
    }
}
