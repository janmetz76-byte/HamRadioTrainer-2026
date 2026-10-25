// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import HamRadioTrainer.Backend

// Der Katalog zum Durchtippen: Pruefungsteil, Abschnitt, Frage. Am Desktop
// stehen Baum, Tabelle und Detail nebeneinander - auf dem Handy nacheinander.
Page {
    id: page

    // Neu laden, wenn sich Pruefung oder Lernstand aendern.
    property int revision: 0
    Connections {
        target: Trainer
        function onProgressChanged() { page.revision++ }
        function onSetupChanged() { browser.pop(null); page.revision++ }
    }

    /// Eine Ebene zurueck; false, wenn schon die oberste erreicht ist.
    function back() {
        if (browser.depth > 1) {
            browser.pop()
            return true
        }
        return false
    }

    component Crumb: RowLayout {
        property alias text: crumbLabel.text
        Layout.fillWidth: true
        ToolButton {
            text: "‹"
            font.pixelSize: 26
            onClicked: browser.pop()
        }
        Label {
            id: crumbLabel
            font.weight: Font.Medium
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }

    StackView {
        id: browser
        anchors.fill: parent
        initialItem: partsView
    }

    Component {
        id: partsView
        ListView {
            model: { page.revision; return Trainer.catalogParts() }
            header: Label {
                text: qsTr("Fragenkatalog")
                font.pixelSize: 22
                font.weight: Font.Medium
                padding: 16
            }
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                contentItem: RowLayout {
                    spacing: 16
                    Label {
                        text: modelData.letter
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        color: Material.primary
                        Layout.preferredWidth: 28
                    }
                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Label {
                            text: modelData.title
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("%1 Fragen").arg(modelData.questions)
                            opacity: 0.6
                            font.pixelSize: 13
                        }
                    }
                    Label { text: "›"; font.pixelSize: 24; opacity: 0.5 }
                }
                onClicked: browser.push(sectionsView, { part: modelData.index, title: modelData.title })
            }
        }
    }

    Component {
        id: sectionsView
        ColumnLayout {
            property int part
            property string title
            spacing: 0
            Crumb { text: title }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: { page.revision; return Trainer.catalogSections(part) }
                delegate: ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    contentItem: RowLayout {
                        spacing: 12
                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            Label {
                                text: modelData.title
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: qsTr("%1 Fragen · %2 beantwortet").arg(modelData.questions).arg(modelData.answered)
                                opacity: 0.6
                                font.pixelSize: 13
                            }
                        }
                        Label { text: "›"; font.pixelSize: 24; opacity: 0.5 }
                    }
                    onClicked: browser.push(questionsView, { part: part, section: modelData.index, title: modelData.title })
                }
            }
        }
    }

    Component {
        id: questionsView
        ColumnLayout {
            property int part
            property int section
            property string title
            spacing: 0
            Crumb { text: title }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: { page.revision; return Trainer.catalogQuestions(part, section) }
                delegate: ItemDelegate {
                    required property var modelData
                    width: ListView.view.width
                    contentItem: ColumnLayout {
                        spacing: 2
                        RowLayout {
                            Label {
                                text: modelData.id
                                font.weight: Font.Bold
                                Layout.fillWidth: true
                            }
                            Label {
                                text: modelData.stage
                                font.pixelSize: 12
                                color: modelData.box < 0 ? Qt.alpha(Material.foreground, 0.5)
                                     : modelData.box === 0 ? "#c62828" : "#2e7d32"
                            }
                        }
                        Label {
                            text: modelData.text
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            wrapMode: Text.WordWrap
                            opacity: 0.8
                            Layout.fillWidth: true
                        }
                    }
                    onClicked: browser.push(detailView, { questionId: modelData.id })
                }
            }
        }
    }

    Component {
        id: detailView
        ColumnLayout {
            id: detail
            property string questionId
            readonly property var question: { page.revision; return Trainer.questionDetail(questionId) }
            spacing: 0
            Crumb { text: detail.questionId }
            ScrollView {
                id: detailScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: detailScroll.availableWidth - 32
                    x: 16
                    spacing: 16
                    QuestionView {
                        question: detail.question
                        correctPosition: detail.question.correctPosition
                        selection: detail.question.correctPosition
                        revealed: true
                        Layout.fillWidth: true
                    }
                    Label {
                        text: detail.question.explanation || ""
                        visible: text !== ""
                        wrapMode: Text.WordWrap
                        opacity: 0.8
                        Layout.fillWidth: true
                    }
                    Label {
                        text: detail.question.timesAnswered > 0
                              ? qsTr("Lernstand: %1 · %2 von %3 Mal richtig").arg(detail.question.stage)
                                    .arg(detail.question.timesCorrect).arg(detail.question.timesAnswered)
                              : qsTr("Lernstand: %1").arg(detail.question.stage || "")
                        opacity: 0.7
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.bottomMargin: 16
                    }
                }
            }
        }
    }
}
