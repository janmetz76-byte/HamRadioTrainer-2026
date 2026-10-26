// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import HamRadioTrainer.Backend

// Die sechs Faelle der Amtsblatt-Verfuegung 29/2024, wie am Desktop. Der
// Katalog folgt aus der Klasse; gelernt wird nur, was der Fall verlangt.
Page {
    id: page
    signal done()
    /// Von der Startseite aus geoeffnet: dann gibt es einen Weg zurueck ohne
    /// Aenderung. Beim ersten Start nicht - ohne Wahl gibt es nichts zu lernen.
    property bool cancelable: false
    signal canceled()

    property var choices: Trainer.combinations()
    property string chosen: Trainer.combinationId !== "" ? Trainer.combinationId : firstAvailable()
    property date examDate: Trainer.suggestedExamDate()
    property bool withDate: !isNaN(Trainer.examDate)

    function firstAvailable() {
        for (const c of choices)
            if (c.available) return c.id
        return ""
    }
    function choice(id) {
        for (const c of choices)
            if (c.id === id) return c
        return null
    }

    header: ToolBar {
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: page.cancelable ? 0 : 16
            ToolButton {
                visible: page.cancelable
                text: "‹"
                font.pixelSize: 28
                onClicked: page.canceled()
            }
            Label {
                text: qsTr("Für welche Prüfung lernen?")
                font.pixelSize: 20
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    // "Weiter" steht fest unten und nicht am Ende der Liste: dort musste man
    // erst darauf kommen, dass sich die Seite noch weiter wischen laesst.
    footer: Rectangle {
        implicitHeight: next.implicitHeight + 24
        color: Material.theme === Material.Dark ? "#262626" : "#f7f7f9"
        Rectangle {
            width: parent.width
            height: 1
            color: Qt.alpha(Material.foreground, 0.12)
        }
        Button {
            id: next
            anchors.fill: parent
            anchors.margins: 12
            text: qsTr("Weiter")
            highlighted: true
            enabled: page.chosen !== ""
            onClicked: {
                Trainer.applySetup(page.chosen, dateSwitch.checked ? page.examDate : new Date(NaN))
                page.done()
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 4

            Label {
                text: qsTr("Gelernt und geprüft werden nur die Prüfungsteile, die dafür nötig sind.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 0
            }

            Repeater {
                model: [
                    { title: qsTr("Prüfung"), ascent: false },
                    { title: qsTr("Aufstieg nach bestandener Klasse"), ascent: true }
                ]
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        text: modelData.title
                        font.weight: Font.Medium
                        color: Material.accent
                        Layout.leftMargin: 16
                        Layout.topMargin: 16
                    }
                    Repeater {
                        model: page.choices.filter(c => c.ascent === modelData.ascent)
                        delegate: RadioDelegate {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.label
                            enabled: modelData.available
                            checked: page.chosen === modelData.id
                            onClicked: page.chosen = modelData.id
                        }
                    }
                }
            }

            Pane {
                Layout.fillWidth: true
                Layout.margins: 16
                Material.elevation: 1
                visible: page.choice(page.chosen) !== null
                ColumnLayout {
                    width: parent.width
                    Label {
                        text: qsTr("Zu lernen")
                        font.weight: Font.Medium
                    }
                    Label {
                        text: {
                            const c = page.choice(page.chosen)
                            return c ? qsTr("%1 – zusammen %2 Fragen.").arg(c.parts).arg(c.total) : ""
                        }
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Label {
                        text: page.choice(page.chosen) ? page.choice(page.chosen).note : ""
                        visible: text !== ""
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                        Layout.fillWidth: true
                    }
                }
            }

            SwitchDelegate {
                id: dateSwitch
                text: qsTr("Prüfungstermin steht fest")
                checked: page.withDate
                Layout.fillWidth: true
            }

            DateChooser {
                visible: dateSwitch.checked
                date: page.examDate
                onDateChanged: page.examDate = date
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: qsTr("Mit Termin plant die App darauf hin: ein Tagesziel für neue Fragen, keine Wiederholung erst nach der Prüfung und in der letzten Woche zuerst, was noch nicht sitzt.")
                wrapMode: Text.WordWrap
                opacity: 0.6
                font.pixelSize: 13
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
