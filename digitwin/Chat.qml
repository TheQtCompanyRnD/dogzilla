import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: mainWindow

    width: 480
    height: 800
    visible: true
    title: qsTr("LlamaChat")

    palette.base: "#0d0e10"
    palette.button: "#0d0e10"
    palette.highlight: "#2a2728"
    palette.buttonText: "#f8fafa"
    palette.text: "#f8fafa"
    color: palette.base

    property var models: []

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: connectView
    }

    Component {
        id: connectView

        Item {
            RowLayout {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 50

                TextField {
                    id: hostField
                    Layout.fillWidth: true
                    text: qsTr("http://localhost:11434")
                }

                Button {
                    text: qsTr("Connect")
                    onClicked: {
                        ollamaApi.apiUrl = hostField.text
                        mainWindow.models = ollamaApi.list()
                        if (mainWindow.models.length === 0)
                            return
                        stackView.push(chatView)
                    }
                }
            }
        }
    }

    Component {
        id: chatView

        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 20

                Item {
                    implicitHeight: 30
                    Layout.fillWidth: true
                    RowLayout {
                        anchors.fill: parent
                        spacing: 4
                        ComboBox {
                            id: modelsComboBox
                            height: parent.height
                            model: mainWindow.models
                            onCurrentTextChanged: ollamaApi.selectedModel = currentText
                        }
                        Item { Layout.fillWidth: true }
                        RoundButton {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("Disconnect")
                            onClicked: {
                                ollamaApi.stop()
                                ollamaApi.model.reset()
                                stackView.pop()
                            }
                        }
                        RoundButton {
                            Layout.alignment: Qt.AlignRight
                            text: qsTr("New Chat")
                            enabled: !ollamaApi.generating
                            visible: ollamaApi.model.size > 0
                            onClicked: {
                                ollamaApi.model.reset()
                                promptTextArea.forceActiveFocus()
                            }
                        }
                    }
                }

                ListView {
                    id: listView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4
                    clip: true

                    ScrollBar.vertical: ScrollBar {
                        anchors.rightMargin: -10
                        policy: ScrollBar.AsNeeded
                    }

                    model: ollamaApi.model

                    delegate: Column {
                        id: conversationDelegate
                        anchors.right: sentByMe ? listView.contentItem.right : undefined
                        spacing: 6

                        required property int index
                        required property var model
                        readonly property string message: model.display
                        readonly property bool sentByMe: model.isUser

                        onMessageChanged: {
                            if (ollamaApi.generating)
                                listView.positionViewAtEnd()
                        }

                        Row {
                            id: messageRow
                            spacing: 6
                            anchors.right: conversationDelegate.sentByMe ? parent.right : undefined

                            Rectangle {
                                anchors.leftMargin: 1
                                width: Math.min(messageText.implicitWidth + 24, listView.width)
                                height: messageText.implicitHeight + 24
                                color: conversationDelegate.sentByMe ? "#323031" : "transparent"

                                TextEdit {
                                    id: messageText
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    readOnly: true
                                    selectByMouse: true
                                    wrapMode: Label.Wrap
                                    text: conversationDelegate.message
                                    color: "white"
                                }
                            }
                        }
                    }
                }

                Item {
                    implicitHeight: 100
                    Layout.fillWidth: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 30
                        color: "#343434"
                        border.width: 1
                        // border.color: "#343434"
                    }

                    TextArea {
                        id: promptTextArea
                        anchors.fill: parent
                        anchors.margins: 8
                        wrapMode: Text.WordWrap
                        // color: "white"
                        enabled: !ollamaApi.generating
                        Keys.onTabPressed: nextItemInFocusChain().forceActiveFocus(Qt.TabFocusReason)
                        Keys.onEnterPressed: event => handleEnter(event)
                        Keys.onReturnPressed: event => handleEnter(event)
                        function handleEnter(event) {
                            if (event.modifiers === Qt.NoModifier ||
                                    event.modifiers === Qt.KeypadModifier) {
                                const text = promptTextArea.text
                                promptTextArea.clear()
                                ollamaApi.send(text)
                                return
                            }
                            event.accepted = false
                        }
                    }

                    RoundButton {
                        id: sendButton
                        anchors.margins: 10
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        hoverEnabled: true
                        enabled: !ollamaApi.generating
                        text: qsTr("Send")
                        onClicked: {
                            const text = promptTextArea.text
                            promptTextArea.clear()
                            ollamaApi.send(text)
                        }
                    }

                    RoundButton {
                        id: stopButton
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.top
                        anchors.bottomMargin: 8
                        visible: ollamaApi.generating
                        onClicked: ollamaApi.stop()
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 8
                            color: "#e4e7eb"
                        }
                    }
                }
            }
        }
    }

    OllamaApi {
        id: ollamaApi

        property string selectedModel: ""
        onSelectedModelChanged: model.name = selectedModel

        function send(text) {
            if (text.length === 0)
                return
            if (selectedModel.length === 0)
                return
            if (model.size === 0)
                startChat(selectedModel)
            ollamaApi.chat(text)
        }
        function stop() {
            if (!generating)
                return
            stopGenerating();
        }
    }
}
