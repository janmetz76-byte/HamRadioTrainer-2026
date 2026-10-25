// SPDX-License-Identifier: GPL-3.0-or-later
// HamRadioTrainer 2026 - Neu aufgebaut von Jan - DC6DX
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

// Tag, Monat und Jahr zum Durchwischen. Ein Kalender braeuchte auf dem Handy
// mehr Platz, als ein Pruefungstermin wert ist.
//
// Die Raeder werden nur beim Laden aus "date" gestellt; danach schreiben sie
// in "date", nicht umgekehrt - sonst stellten sie sich gegenseitig zurueck.
RowLayout {
    id: chooser
    property date date: new Date()
    spacing: 8

    readonly property int firstYear: new Date().getFullYear()
    property bool ready: false

    function daysIn(month, year) { return new Date(year, month + 1, 0).getDate() }
    function update() {
        if (!ready)
            return
        const year = firstYear + yearTumbler.currentIndex
        const month = monthTumbler.currentIndex
        const day = Math.min(dayTumbler.currentIndex + 1, daysIn(month, year))
        chooser.date = new Date(year, month, day)
    }

    Component.onCompleted: {
        dayTumbler.currentIndex = chooser.date.getDate() - 1
        monthTumbler.currentIndex = chooser.date.getMonth()
        yearTumbler.currentIndex = Math.max(0, chooser.date.getFullYear() - firstYear)
        ready = true
    }

    component Wheel: Label {
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: 1.0 - Math.abs(Tumbler.displacement) / 2
        font.pixelSize: 18
    }

    Tumbler {
        id: dayTumbler
        model: 31
        visibleItemCount: 3
        delegate: Wheel {
            required property int index
            text: index + 1
        }
        onCurrentIndexChanged: chooser.update()
    }
    Tumbler {
        id: monthTumbler
        model: [qsTr("Januar"), qsTr("Februar"), qsTr("März"), qsTr("April"), qsTr("Mai"),
                qsTr("Juni"), qsTr("Juli"), qsTr("August"), qsTr("September"), qsTr("Oktober"),
                qsTr("November"), qsTr("Dezember")]
        visibleItemCount: 3
        implicitWidth: 120
        delegate: Wheel {
            required property string modelData
            required property int index
            text: modelData
        }
        onCurrentIndexChanged: chooser.update()
    }
    Tumbler {
        id: yearTumbler
        model: 4
        visibleItemCount: 3
        delegate: Wheel {
            required property int index
            text: chooser.firstYear + index
        }
        onCurrentIndexChanged: chooser.update()
    }
}
