// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

// Eine Frage mit ihren Antworten als grosse Flaechen zum Antippen. Vor der
// Aufloesung ist die gewaehlte Antwort markiert, danach die richtige gruen
// und eine falsch gewaehlte rot.
ColumnLayout {
    id: view
    property var question: ({})
    property int selection: -1
    property int correctPosition: -1
    property bool revealed: false
    signal answerChosen(int position)

    spacing: 10

    Label {
        text: view.question.text || ""
        textFormat: Text.RichText
        wrapMode: Text.WordWrap
        font.pixelSize: 18
        Layout.fillWidth: true
    }

    Image {
        source: view.question.image || ""
        visible: source != ""
        fillMode: Image.PreserveAspectFit
        sourceSize.width: width * Screen.devicePixelRatio
        Layout.fillWidth: true
        Layout.preferredHeight: visible && implicitWidth > 0 ? width * implicitHeight / implicitWidth : 0
    }

    Repeater {
        model: view.question.answers || []
        delegate: ItemDelegate {
            id: answer
            required property var modelData
            required property int index

            readonly property bool isChosen: index === view.selection
            readonly property bool isRight: view.revealed && index === view.correctPosition
            readonly property bool isWrong: view.revealed && isChosen && !isRight

            Layout.fillWidth: true
            padding: 12
            enabled: !view.revealed || isChosen || isRight
            onClicked: view.answerChosen(index)

            background: Rectangle {
                radius: 10
                color: answer.isRight ? "#d7f0d9"
                     : answer.isWrong ? "#f8d6d6"
                     : answer.isChosen ? Qt.alpha(Material.primary, 0.14)
                     : Material.theme === Material.Dark ? "#2a2a2a" : "#f4f4f6"
                border.width: answer.isChosen || answer.isRight ? 2 : 1
                border.color: answer.isRight ? "#2e7d32"
                            : answer.isWrong ? "#c62828"
                            : answer.isChosen ? Material.primary
                            : Qt.alpha(Material.foreground, 0.15)
            }

            contentItem: RowLayout {
                spacing: 12
                Rectangle {
                    width: 34
                    height: 34
                    radius: 17
                    color: answer.isRight ? "#2e7d32" : answer.isWrong ? "#c62828"
                         : answer.isChosen ? Material.primary : "transparent"
                    border.width: 1
                    border.color: answer.isChosen || answer.isRight || answer.isWrong
                                  ? "transparent" : Qt.alpha(Material.foreground, 0.4)
                    Layout.alignment: Qt.AlignTop
                    Label {
                        anchors.centerIn: parent
                        text: answer.isRight ? "✓" : answer.isWrong ? "×" : answer.modelData.letter
                        font.weight: Font.Bold
                        color: answer.isChosen || answer.isRight || answer.isWrong ? "white" : Material.foreground
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        text: answer.modelData.text
                        visible: text !== ""
                        textFormat: Text.RichText
                        wrapMode: Text.WordWrap
                        font.pixelSize: 16
                        color: answer.isRight || answer.isWrong ? "#1b1b1b" : Material.foreground
                        Layout.fillWidth: true
                    }
                    Image {
                        source: answer.modelData.image
                        visible: source != ""
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: width * Screen.devicePixelRatio
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible && implicitWidth > 0 ? width * implicitHeight / implicitWidth : 0
                    }
                }
            }
        }
    }
}
