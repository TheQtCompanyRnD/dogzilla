import QtQuick
import QtQuick.Controls

ListView {
    id: listView
    width: 320
    spacing: 4
    clip: true

    ScrollBar.vertical: ScrollBar {
        anchors.rightMargin: -10
        policy: ScrollBar.AsNeeded
    }

    delegate: Row {
        id: conversationDelegate
        anchors.right: source ? undefined : listView.contentItem.right
        spacing: 6

        required property int index
        required property var model
        readonly property string message: model.display
        readonly property int source: model.source

        Rectangle {
            anchors.leftMargin: 1
            width: Math.min(messageText.implicitWidth + 24, listView.width)
            height: messageText.implicitHeight + 24
            color: conversationDelegate.source ? "#323031" : "transparent"

            Text {
                id: messageText
                anchors.fill: parent
                anchors.margins: 12
                wrapMode: Label.Wrap
                text: conversationDelegate.message
                color: "white"
            }
        }
    }
}
