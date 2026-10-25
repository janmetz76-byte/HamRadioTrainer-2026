// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import HamRadioTrainer.Backend

ApplicationWindow {
    id: window
    // Am Desktop zeigt die Vorschau Handygroesse; unter Android fuellt das
    // Fenster ohnehin den Bildschirm.
    width: 412
    height: 900
    visible: true
    title: qsTr("HamRadioTrainer 2026")

    Material.theme: Material.System

    // Die Zurueck-Geste von Android schliesst unter Qt das Fenster. Ist eine
    // Seite daruebergeschoben, soll sie nur diese schliessen.
    //
    // Ohne aufgelegte Seite geht die Geste erst innerhalb des Katalogs zurueck,
    // dann zur Startseite; erst von dort beendet sie die App.
    onClosing: close => {
        if (stack.depth > 1) {
            close.accepted = false
            stack.pop()
        } else if (stack.currentItem && stack.currentItem.back && stack.currentItem.back()) {
            close.accepted = false
        }
    }
    Material.primary: "#1f4e79"
    Material.accent: "#2e7d32"

    // Ohne Einrichtung gibt es nichts zu lernen: dann steht der Assistent
    // vorn, wie beim ersten Start am Desktop.
    StackView {
        id: stack
        anchors.fill: parent
        initialItem: Trainer.setupDone ? tabs : setup
    }

    Component {
        id: setup
        SetupPage {
            cancelable: stack.depth > 1
            onDone: stack.replace(null, tabs)
            onCanceled: stack.pop()
        }
    }

    Component {
        id: tabs
        Page {
            function back() {
                if (bar.currentIndex === 4 && catalogPage.back())
                    return true
                if (bar.currentIndex !== 0) {
                    bar.currentIndex = 0
                    return true
                }
                return false
            }

            header: ToolBar {
                Material.foreground: "white"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    Label {
                        text: [qsTr("HamRadioTrainer 2026"), qsTr("Lernen"), qsTr("Prüfung"),
                               qsTr("Statistik"), qsTr("Katalog")][bar.currentIndex]
                        font.pixelSize: 20
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        text: qsTr("Prüfung wählen")
                        visible: bar.currentIndex === 0
                        onClicked: stack.push(setup)
                    }
                }
            }

            StackLayout {
                anchors.fill: parent
                currentIndex: bar.currentIndex
                StartPage {
                    onLearnRequested: bar.currentIndex = 1
                }
                LearnPage {}
                ExamPage {}
                StatsPage {}
                CatalogPage { id: catalogPage }
            }

            footer: TabBar {
                id: bar
                objectName: "tabBar"
                TabButton { text: qsTr("Start") }
                TabButton { text: qsTr("Lernen") }
                TabButton { text: qsTr("Prüfung") }
                TabButton { text: qsTr("Statistik") }
                TabButton { text: qsTr("Katalog") }
            }
        }
    }
}
