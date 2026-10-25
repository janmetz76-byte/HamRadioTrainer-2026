// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import HamRadioTrainer.Backend

// Pruefungssimulation wie am Desktop: ein Bogen aus den gewaehlten Teilen,
// eine Uhr, Bestehen ab 19 von 25 Punkten je Teil. Waehrend der Pruefung gibt
// es keine Aufloesung - die steht erst im Ergebnis.
Page {
    id: page
    readonly property var exam: Trainer.exam

    function clock(seconds) {
        const h = Math.floor(seconds / 3600)
        const m = Math.floor(seconds % 3600 / 60)
        const s = seconds % 60
        return (h > 0 ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(s).padStart(2, "0")
    }

    // ---- Vor der Pruefung -------------------------------------------------
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        visible: !page.exam.running && !page.exam.finished

        ColumnLayout {
            width: parent.width
            spacing: 12

            Label {
                text: qsTr("Prüfungssimulation")
                font.pixelSize: 22
                font.weight: Font.Medium
                Layout.margins: 16
                Layout.bottomMargin: 0
            }
            Label {
                text: qsTr("Wie in der Prüfung bei der Bundesnetzagentur: 25 Fragen je Prüfungsteil, bestanden ab 19 Punkten in jedem Teil. Unbeantwortete Fragen zählen als falsch.")
                wrapMode: Text.WordWrap
                opacity: 0.75
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
            }

            Repeater {
                model: page.exam.plan.parts || []
                delegate: Pane {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Material.elevation: 1
                    RowLayout {
                        width: parent.width
                        Label {
                            text: modelData.letter
                            font.pixelSize: 22
                            font.weight: Font.Bold
                            color: Material.primary
                            Layout.preferredWidth: 32
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: modelData.name
                                font.weight: Font.Medium
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: qsTr("%1 Fragen · %2 Minuten · bestanden ab %3 Punkten")
                                          .arg(modelData.questions).arg(modelData.minutes).arg(modelData.passScore)
                                opacity: 0.7
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }

            Label {
                text: qsTr("Zusammen %1 Fragen in %2 Minuten.").arg(page.exam.plan.questions || 0).arg(page.exam.plan.minutes || 0)
                font.weight: Font.Medium
                Layout.leftMargin: 16
            }
            Label {
                text: page.exam.message
                visible: text !== ""
                color: "#c62828"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
            }
            Item { Layout.preferredHeight: 8 }
        }
    }

    // ---- Waehrend der Pruefung --------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        visible: page.exam.running
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 4
            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Label {
                    text: qsTr("Teil %1 · %2").arg(page.exam.partLetter).arg(page.exam.partName)
                    elide: Text.ElideRight
                    font.weight: Font.Medium
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("Frage %1 von %2 · %3 beantwortet").arg(page.exam.index + 1)
                              .arg(page.exam.count).arg(page.exam.answeredCount)
                    opacity: 0.7
                    font.pixelSize: 13
                }
            }
            Label {
                text: page.clock(page.exam.secondsLeft)
                font.pixelSize: 20
                font.weight: Font.Bold
                color: page.exam.secondsLeft < 300 ? "#c62828" : Material.foreground
            }
            // Drei gezeichnete Striche statt "☰": das Zeichen fehlt der
            // Android-Systemschrift und erschien als leeres Kaestchen.
            ToolButton {
                onClicked: overview.open()
                contentItem: Item {
                    implicitWidth: 24
                    implicitHeight: 24
                    Column {
                        anchors.centerIn: parent
                        spacing: 4
                        Repeater {
                            model: 3
                            Rectangle {
                                width: 20
                                height: 2.5
                                radius: 1
                                color: Material.foreground
                            }
                        }
                    }
                }
            }
        }
        ProgressBar {
            from: 0
            to: Math.max(1, page.exam.count)
            value: page.exam.answeredCount
            Layout.fillWidth: true
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 100
            contentWidth: availableWidth
            QuestionView {
                width: scroll.availableWidth - 32
                x: 16
                y: 16
                question: page.exam.question
                selection: page.exam.selection
                onAnswerChosen: position => page.exam.select(position)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: navigation.implicitHeight + 24
            color: Material.theme === Material.Dark ? "#262626" : "#f7f7f9"
            Rectangle {
                width: parent.width
                height: 1
                color: Qt.alpha(Material.foreground, 0.12)
            }
            RowLayout {
                id: navigation
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Button {
                    text: qsTr("Zurück")
                    enabled: page.exam.index > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    onClicked: { page.exam.previous(); scroll.ScrollBar.vertical.position = 0 }
                }
                Button {
                    text: page.exam.index + 1 < page.exam.count ? qsTr("Weiter") : qsTr("Abgeben")
                    highlighted: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    onClicked: {
                        if (page.exam.index + 1 < page.exam.count) {
                            page.exam.next()
                            scroll.ScrollBar.vertical.position = 0
                        } else {
                            confirm.ask(true)
                        }
                    }
                }
            }
        }
    }

    // Alle Fragen auf einen Blick: beantwortete gefuellt, offene leer.
    Drawer {
        id: overview
        edge: Qt.RightEdge
        width: Math.min(page.width * 0.85, 360)
        // Ueber die ganze Fensterhoehe, nicht nur die Seite: sonst schauten
        // Schaltflaechen und Tabs unten heraus.
        height: page.ApplicationWindow.window ? page.ApplicationWindow.window.height : page.height
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12
            Label {
                text: qsTr("Fragenübersicht")
                font.pixelSize: 20
                font.weight: Font.Medium
            }
            Label {
                text: qsTr("%1 von %2 beantwortet").arg(page.exam.answeredCount).arg(page.exam.count)
                opacity: 0.7
            }
            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                cellWidth: width / 5
                cellHeight: 52
                model: page.exam.answered
                delegate: Item {
                    required property int index
                    required property bool modelData
                    width: grid.cellWidth
                    height: grid.cellHeight
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: 8
                        color: modelData ? Material.primary : "transparent"
                        border.width: index === page.exam.index ? 3 : 1
                        border.color: index === page.exam.index ? Material.accent : Qt.alpha(Material.foreground, 0.3)
                        Label {
                            anchors.centerIn: parent
                            text: index + 1
                            color: modelData ? "white" : Material.foreground
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { page.exam.go(index); overview.close() }
                    }
                }
            }
            Button {
                text: qsTr("Abgeben")
                highlighted: true
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                onClicked: { overview.close(); confirm.ask(true) }
            }
            Button {
                text: qsTr("Prüfung abbrechen")
                flat: true
                Layout.fillWidth: true
                onClicked: { overview.close(); confirm.ask(false) }
            }
        }
    }

    // Eine Rueckfrage fuer Abgeben und Abbrechen. Der Text wird erst beim
    // Oeffnen gesetzt: ein schon beim Aufbau umbrechender Text liess den
    // Material-Dialog seine Hoehe im Kreis rechnen.
    Dialog {
        id: confirm
        property string message
        property bool handingIn: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(page.width - 48, 400)
        modal: true
        // Eigene Knoepfe: die Standardknoepfe von Qt heissen ohne geladene
        // Qt-Uebersetzung "Yes" und "No".
        footer: DialogButtonBox {
            Button {
                text: qsTr("Nein")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                text: qsTr("Ja")
                flat: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        contentItem: Label {
            wrapMode: Text.WordWrap
            text: confirm.message
        }
        onAccepted: handingIn ? page.exam.finish() : page.exam.abort()

        function ask(handIn) {
            handingIn = handIn
            const open = page.exam.count - page.exam.answeredCount
            title = handIn ? qsTr("Prüfung abgeben?") : qsTr("Prüfung abbrechen?")
            message = !handIn ? qsTr("Der Bogen wird verworfen und nicht ausgewertet.")
                    : open > 0 ? qsTr("%1 Fragen sind noch offen und werden als falsch gewertet.").arg(open)
                    : qsTr("Alle Fragen sind beantwortet.")
            confirm.open()
        }
    }

    // ---- Ergebnis ---------------------------------------------------------
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        visible: page.exam.finished

        ColumnLayout {
            width: parent.width
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: verdict.implicitHeight + 40
                color: page.exam.result.passed ? "#2e7d32" : "#c62828"
                ColumnLayout {
                    id: verdict
                    anchors.centerIn: parent
                    width: parent.width - 32
                    Label {
                        text: page.exam.result.passed ? qsTr("Bestanden") : qsTr("Nicht bestanden")
                        color: "white"
                        font.pixelSize: 28
                        font.weight: Font.Bold
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Label {
                        text: qsTr("%1 von %2 Punkten").arg(page.exam.result.achieved || 0).arg(page.exam.result.max || 0)
                        color: "white"
                        font.pixelSize: 18
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Label {
                visible: !page.exam.result.passed && (page.exam.result.failedParts || "") !== ""
                // Muendlich nur bei genau einem knapp verfehlten Teil (Verfuegung 7.3).
                text: (page.exam.result.oralRetestPart || "") !== ""
                      ? qsTr("Nicht bestanden: Prüfungsteil %1. Mit mindestens 17 Punkten in nur diesem Teil ist eine mündliche Nachprüfung möglich; die übrigen Teile müssen nicht wiederholt werden.")
                            .arg(page.exam.result.oralRetestPart)
                      : qsTr("Nicht bestanden: Prüfungsteil %1. Nicht bestandene Teile lassen sich innerhalb von 24 Monaten wiederholen; bestandene Teile bleiben gültig.")
                            .arg(page.exam.result.failedParts || "")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
            }

            Repeater {
                model: page.exam.result.parts || []
                delegate: Pane {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Material.elevation: 1
                    RowLayout {
                        width: parent.width
                        Label {
                            text: modelData.passed ? "✓" : "×"
                            color: modelData.passed ? "#2e7d32" : "#c62828"
                            font.pixelSize: 24
                            font.weight: Font.Bold
                            Layout.preferredWidth: 32
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: qsTr("Teil %1 · %2").arg(modelData.letter).arg(modelData.name)
                                font.weight: Font.Medium
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: qsTr("%1 von %2 Punkten, bestanden ab %3 · %4 falsch · %5 offen")
                                          .arg(modelData.achieved).arg(modelData.max).arg(modelData.passScore)
                                          .arg(modelData.wrong).arg(modelData.unanswered)
                                opacity: 0.7
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
            Label {
                text: qsTr("Die Antworten sind in den Lernfortschritt eingegangen.")
                opacity: 0.6
                font.pixelSize: 13
                Layout.leftMargin: 16
            }
            Item { Layout.preferredHeight: 8 }
        }
    }

    // Die Hauptschaltflaeche steht fest unten - vor der Pruefung zum Starten,
    // danach zum Zurueckkehren.
    footer: Rectangle {
        visible: !page.exam.running
        implicitHeight: visible ? action.implicitHeight + 24 : 0
        color: Material.theme === Material.Dark ? "#262626" : "#f7f7f9"
        Rectangle {
            width: parent.width
            height: 1
            color: Qt.alpha(Material.foreground, 0.12)
        }
        Button {
            id: action
            anchors.fill: parent
            anchors.margins: 12
            highlighted: true
            text: page.exam.finished ? qsTr("Neue Prüfung") : qsTr("Prüfung starten")
            onClicked: {
                if (page.exam.finished)
                    page.exam.dismissResult()
                else
                    page.exam.start()
            }
        }
    }
}
