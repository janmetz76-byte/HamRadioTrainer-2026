// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import HamRadioTrainer.Backend

// Wo stehe ich, und was kommt als Naechstes - auf einen Blick.
Page {
    id: page
    signal learnRequested()

    readonly property var stats: Trainer.statistics

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 12

            Pane {
                Layout.fillWidth: true
                Layout.margins: 16
                Layout.bottomMargin: 0
                Material.elevation: 2
                ColumnLayout {
                    width: parent.width
                    spacing: 6
                    Label {
                        text: Trainer.combinationLabel
                        font.pixelSize: 26
                        font.weight: Font.Bold
                    }
                    Label {
                        text: Trainer.partsText
                        wrapMode: Text.WordWrap
                        opacity: 0.75
                        Layout.fillWidth: true
                    }
                    Label {
                        text: Trainer.daysToExam > 0 ? qsTr("Noch %1 Tage bis zur Prüfung").arg(Trainer.daysToExam)
                            : Trainer.daysToExam === 0 ? qsTr("Heute ist Prüfungstag")
                            : qsTr("Kein Prüfungstermin hinterlegt")
                        color: Material.accent
                        font.weight: Font.Medium
                        Layout.topMargin: 6
                    }
                }
            }

            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Material.elevation: 1
                ColumnLayout {
                    width: parent.width
                    spacing: 8
                    Label {
                        text: qsTr("Lernfortschritt")
                        font.weight: Font.Medium
                    }
                    ProgressBar {
                        from: 0
                        to: 100
                        value: page.stats.coverage || 0
                        Layout.fillWidth: true
                    }
                    Label {
                        text: qsTr("%1 von %2 Fragen beantwortet").arg(page.stats.answered || 0).arg(page.stats.questions || 0)
                    }
                    GridLayout {
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 4
                        Layout.topMargin: 4
                        Label { text: qsTr("Neu") ; opacity: 0.7 }
                        Label { text: page.stats.fresh || 0 ; font.weight: Font.Medium }
                        Label { text: qsTr("Wackelig") ; opacity: 0.7 }
                        Label { text: page.stats.shaky || 0 ; font.weight: Font.Medium }
                        Label { text: qsTr("Heute fällig") ; opacity: 0.7 }
                        Label { text: page.stats.due || 0 ; font.weight: Font.Medium }
                        Label { text: qsTr("Gefestigt") ; opacity: 0.7 }
                        Label { text: page.stats.mastered || 0 ; font.weight: Font.Medium }
                        Label { text: qsTr("Trefferquote") ; opacity: 0.7 }
                        Label { text: Math.round(page.stats.accuracy || 0) + " %" ; font.weight: Font.Medium }
                    }
                }
            }

            // Was der Pruefungstermin heute verlangt. Ohne Termin nicht da.
            Pane {
                id: planPane
                readonly property var plan: page.stats.plan || {}
                visible: plan.active === true
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Material.elevation: 1
                ColumnLayout {
                    width: parent.width
                    spacing: 8
                    Label {
                        text: qsTr("Bis zur Prüfung")
                        font.weight: Font.Medium
                    }
                    Label {
                        readonly property var plan: planPane.plan
                        text: plan.fresh === 0 && plan.newToday === 0
                                  ? qsTr("Alle Fragen sind mindestens einmal gelernt. Jetzt wiederholen, was noch nicht sitzt.")
                              : plan.finalPhase
                                  ? qsTr("Schlussphase: Unsicheres und Falsches kommt zuerst, danach die %1 noch nie gefragten Fragen.").arg(plan.fresh)
                              : plan.newRemainingToday === 0
                                  ? qsTr("Tagesziel erreicht: %1 neue Fragen heute.").arg(plan.newToday)
                                  : qsTr("Tagesziel: %1 neue Fragen, davon %2 geschafft.").arg(plan.dailyNewTarget).arg(plan.newToday)
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    ProgressBar {
                        from: 0
                        to: 100
                        value: planPane.plan.readiness || 0
                        Layout.fillWidth: true
                    }
                    Label {
                        text: qsTr("Prüfungsreife: %1 % der Fragen gefestigt").arg(Math.round(planPane.plan.readiness || 0))
                        opacity: 0.75
                    }
                }
            }

            Button {
                text: qsTr("Weiterlernen")
                highlighted: true
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.preferredHeight: 56
                onClicked: page.learnRequested()
            }
        }
    }
}
