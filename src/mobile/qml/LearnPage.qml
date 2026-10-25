// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import HamRadioTrainer.Backend

// Eine Frage je Bildschirm. Ablauf wie im Trainer von 2015: Antwort waehlen,
// Sicherheit einschaetzen, dann die Aufloesung.
Page {
    id: page
    readonly property var round: Trainer.study

    // ---- Vor der Runde: Einstellungen -----------------------------------
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        visible: !page.round.running && !page.round.finished

        ColumnLayout {
            width: parent.width
            spacing: 0

            Label {
                text: qsTr("Übungsrunde")
                font.pixelSize: 22
                font.weight: Font.Medium
                Layout.margins: 16
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Label { text: qsTr("Fragen je Runde"); Layout.fillWidth: true }
                SpinBox {
                    from: 1
                    to: 200
                    value: page.round.roundSize
                    onValueModified: page.round.roundSize = value
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Label { text: qsTr("Prüfungsteil"); Layout.fillWidth: true }
                ComboBox {
                    id: partBox
                    textRole: "title"
                    valueRole: "letter"
                    model: [{ letter: "", title: qsTr("alle") }].concat(page.round.partChoices)
                    Component.onCompleted: currentIndex = Math.max(0, indexOfValue(page.round.partFilter))
                    onActivated: page.round.partFilter = currentValue
                    Layout.preferredWidth: 200
                }
            }

            SwitchDelegate {
                text: qsTr("Nur fällige Wiederholungen")
                checked: page.round.onlyDue
                onToggled: page.round.onlyDue = checked
                Layout.fillWidth: true
            }
            SwitchDelegate {
                text: qsTr("Nur wackelige Fragen")
                checked: page.round.onlyShaky
                onToggled: page.round.onlyShaky = checked
                Layout.fillWidth: true
            }
            SwitchDelegate {
                text: qsTr("Lösung nach jeder Antwort zeigen")
                checked: page.round.showSolution
                onToggled: page.round.showSolution = checked
                Layout.fillWidth: true
            }

            Label {
                text: page.round.message
                visible: text !== ""
                color: "#c62828"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.margins: 16
            }

            Button {
                text: qsTr("Runde starten")
                highlighted: true
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.preferredHeight: 56
                onClicked: page.round.start()
            }
        }
    }

    // ---- Waehrend der Runde: eine Frage ---------------------------------
    ColumnLayout {
        anchors.fill: parent
        visible: page.round.running
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 4
            Label {
                text: qsTr("Frage %1 von %2 · Teil %3").arg(page.round.index + 1)
                          .arg(page.round.count).arg(page.round.partLetter)
                opacity: 0.7
                Layout.fillWidth: true
            }
            ToolButton {
                text: qsTr("Beenden")
                onClicked: page.round.stop()
            }
        }
        ProgressBar {
            from: 0
            to: Math.max(1, page.round.count)
            value: page.round.index + (page.round.revealed ? 1 : 0)
            Layout.fillWidth: true
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Ohne kleine Wunschhoehe nimmt der Bereich die volle Hoehe der
            // Frage und schiebt die Knoepfe darunter hinter die Tab-Leiste.
            Layout.preferredHeight: 100
            contentWidth: availableWidth
            QuestionView {
                width: scroll.availableWidth - 32
                x: 16
                y: 16
                question: page.round.question
                selection: page.round.selection
                correctPosition: page.round.correctPosition
                revealed: page.round.revealed
                onAnswerChosen: position => page.round.select(position)
            }
        }

        // Unten, wo der Daumen ist: erst die Einschaetzung, dann "weiter".
        // Die Hoehe folgt ausdruecklich dem Inhalt; ein Pane rechnete sie
        // hier zu knapp, und "Naechste Frage" verschwand hinter den Tabs.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: actions.implicitHeight + 24
            color: Material.theme === Material.Dark ? "#262626" : "#f7f7f9"
            Rectangle {
                width: parent.width
                height: 1
                color: Qt.alpha(Material.foreground, 0.12)
            }
            ColumnLayout {
                id: actions
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Label {
                    text: page.round.feedback
                    visible: page.round.revealed
                    font.weight: Font.Medium
                    color: page.round.selection === page.round.correctPosition ? "#2e7d32" : "#c62828"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    text: page.round.question.explanation || ""
                    visible: page.round.revealed && text !== ""
                    wrapMode: Text.WordWrap
                    opacity: 0.8
                    Layout.fillWidth: true
                }

                Label {
                    text: page.round.selection < 0 ? qsTr("Antwort antippen") : qsTr("Wie sicher war die Antwort?")
                    visible: !page.round.revealed
                    opacity: 0.7
                }
                RowLayout {
                    visible: !page.round.revealed
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        // Farben wie die Lernstufen: sicher gruen, unsicher
                        // bernstein, geraten grau.
                        model: [
                            { text: qsTr("Sicher"), rating: StudyRound.Sure, color: "#2e7d32" },
                            { text: qsTr("Unsicher"), rating: StudyRound.Unsure, color: "#ef8f00" },
                            { text: qsTr("Geraten"), rating: StudyRound.Guessed, color: "#757575" }
                        ]
                        delegate: Button {
                            required property var modelData
                            text: modelData.text
                            enabled: page.round.selection >= 0
                            Material.background: enabled ? modelData.color : undefined
                            Material.foreground: enabled ? "white" : undefined
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            onClicked: page.round.rate(modelData.rating)
                        }
                    }
                }
                Button {
                    text: qsTr("Nächste Frage")
                    visible: page.round.revealed
                    highlighted: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    onClicked: {
                        page.round.next()
                        scroll.ScrollBar.vertical.position = 0
                    }
                }
            }
        }
    }

    // ---- Nach der Runde --------------------------------------------------
    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width - 32
        visible: page.round.finished
        spacing: 16

        Label {
            text: qsTr("Runde beendet")
            font.pixelSize: 24
            font.weight: Font.Bold
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("%1 von %2 Fragen richtig").arg(page.round.correctCount).arg(page.round.count)
            font.pixelSize: 18
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("%1 Fragen waren neu dabei").arg(page.round.freshCount)
            opacity: 0.7
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            text: qsTr("Neue Runde")
            highlighted: true
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            onClicked: page.round.start()
        }
        Button {
            text: qsTr("Einstellungen")
            flat: true
            Layout.fillWidth: true
            onClicked: page.round.stop()
        }
    }
}
