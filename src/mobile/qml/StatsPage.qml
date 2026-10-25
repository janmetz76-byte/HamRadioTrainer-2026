// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import HamRadioTrainer.Backend

// Dieselben Zahlen wie die Statistik am Desktop, als Balken statt Tabellen.
Page {
    id: page
    readonly property var stats: Trainer.statistics
    readonly property var stageNames: [qsTr("Neu"), qsTr("Stufe 1"), qsTr("Stufe 2"),
                                       qsTr("Stufe 3"), qsTr("Stufe 4"), qsTr("Gefestigt")]
    readonly property var stageColors: ["#9e9e9e", "#c62828", "#ef6c00", "#f9a825", "#7cb342", "#2e7d32"]

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 12

            Label {
                text: qsTr("Lernstufen")
                font.pixelSize: 20
                font.weight: Font.Medium
                Layout.leftMargin: 16
                Layout.topMargin: 16
            }
            Repeater {
                model: page.stats.boxes || []
                delegate: RowLayout {
                    required property int modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    spacing: 12
                    Label {
                        text: page.stageNames[index]
                        Layout.preferredWidth: 90
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 14
                        radius: 7
                        color: Qt.alpha(Material.foreground, 0.08)
                        Rectangle {
                            width: parent.width * (page.stats.answered > 0 ? modelData / page.stats.answered : 0)
                            height: parent.height
                            radius: 7
                            color: page.stageColors[index]
                        }
                    }
                    Label {
                        text: modelData
                        horizontalAlignment: Text.AlignRight
                        Layout.preferredWidth: 40
                    }
                }
            }

            Label {
                text: qsTr("Je Prüfungsteil")
                font.pixelSize: 20
                font.weight: Font.Medium
                Layout.leftMargin: 16
                Layout.topMargin: 12
            }
            Repeater {
                model: page.stats.parts || []
                delegate: Pane {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Material.elevation: 1
                    ColumnLayout {
                        width: parent.width
                        spacing: 6
                        Label {
                            text: modelData.title
                            font.weight: Font.Medium
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        ProgressBar {
                            from: 0
                            to: Math.max(1, modelData.questions)
                            value: modelData.answered
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("%1 von %2 beantwortet · %3 gefestigt · %4 wackelig · Trefferquote %5 %")
                                      .arg(modelData.answered).arg(modelData.questions)
                                      .arg(modelData.mastered).arg(modelData.shaky)
                                      .arg(Math.round(modelData.accuracy))
                            wrapMode: Text.WordWrap
                            opacity: 0.75
                            font.pixelSize: 13
                            Layout.fillWidth: true
                        }
                    }
                }
            }
            Item { Layout.preferredHeight: 16 }
        }
    }
}
