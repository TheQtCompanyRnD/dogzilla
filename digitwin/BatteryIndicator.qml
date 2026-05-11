import QtQuick
import QtQuick.Shapes

Shape {
    id: root
    required property real level // from 0 to 1

    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        strokeColor: "black"
        fillColor: "white"
        pathHints: ShapePath.PathNonIntersecting | ShapePath.PathNonOverlappingControlPointTriangles
        PathSvg {
            path: "m 33.750146,6.1416157 c 0,-0.5935737 -0.346882,-1.0955942 -0.776958,-1.0714332 H 32.18915 V 2.1428663
                C 32.18915,0.95571839 31.496103,0 30.635233,0 H 1.5539165 C 0.69304676,0 0,0.95571839 0,2.1428663 V 12.857198
                c 0,1.187148 0.69304676,2.142867 1.5539165,2.142867 H 30.635233 c 0.86087,0 1.553917,-0.955719 1.553917,-2.142867
                V 9.9298819 h 0.784038 c 0.430435,0 0.776958,-0.477859 0.776958,-1.0714333 z"
        }
    }

    Rectangle {
        x: 2
        width: 28 * root.level
        height: 11
        anchors.verticalCenter: parent.verticalCenter
        radius: 1
        color: "YellowGreen"

        Text {
            x: 3
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 10
            text: Math.round(root.level * 100) + "%"
        }
    }
}
