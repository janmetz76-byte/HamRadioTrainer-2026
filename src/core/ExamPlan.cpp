// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ExamPlan.h"

#include <QCoreApplication>

namespace hrt {
namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("ExamPlan", text);
}

} // namespace

QVector<int> partsForClass(const QString &examClass)
{
    // Amtsblatt-Verfügung 29/2024. Klasse N besteht aus den Teilen V, B und N,
    // Klasse E aus V, B, N und E, Klasse A aus allen fuenf.
    if (examClass == QStringLiteral("N"))
        return {0, 1, 2};
    if (examClass == QStringLiteral("E"))
        return {0, 1, 2, 3};
    return {0, 1, 2, 3, 4};
}

QVector<ExamCombination> examCombinations()
{
    // Reihenfolge: erst die Pruefungen von vorn, danach die Aufstiege. Das ist
    // die Reihenfolge, in der jemand sie erlebt.
    return {
        {QStringLiteral("N"), tr("Klasse N"), QString(), QStringLiteral("N"),
         partsForClass(QStringLiteral("N")), false},
        {QStringLiteral("E"), tr("Klasse E"),
         tr("Die Technik Klasse E baut auf der Technik Klasse N auf; geprüft werden beide."),
         QStringLiteral("E"), partsForClass(QStringLiteral("E")), false},
        {QStringLiteral("A"), tr("Klasse A"),
         tr("Die Technik Klasse A baut auf den Klassen N und E auf; geprüft werden alle drei."),
         QStringLiteral("A"), partsForClass(QStringLiteral("A")), false},
        {QStringLiteral("N-E"), tr("Von Klasse N nach E"),
         tr("Vorschriften, Betriebstechnik und Technik Klasse N sind mit der Klasse N "
            "bestanden und werden nicht erneut geprüft."),
         QStringLiteral("E"), {3}, true},
        {QStringLiteral("N-A"), tr("Von Klasse N nach A"),
         tr("Vorschriften, Betriebstechnik und Technik Klasse N sind mit der Klasse N "
            "bestanden. Für die Klasse A wird zusätzlich die Technik Klasse E geprüft."),
         QStringLiteral("A"), {3, 4}, true},
        {QStringLiteral("E-A"), tr("Von Klasse E nach A"),
         tr("Vorschriften, Betriebstechnik und die Technik der Klassen N und E sind mit "
            "der Klasse E bestanden und werden nicht erneut geprüft."),
         QStringLiteral("A"), {4}, true},
    };
}

ExamCombination examCombination(const QString &id)
{
    for (const ExamCombination &combination : examCombinations()) {
        if (combination.id == id)
            return combination;
    }
    return {};
}

QString examPartName(int part)
{
    // "V + B + N" sagt einem Lernenden nichts, der die Verfuegung nicht kennt.
    switch (part) {
    case 0:
        return tr("Vorschriften");
    case 1:
        return tr("Betriebstechnik");
    case 2:
        return tr("Technik Klasse N");
    case 3:
        return tr("Technik Klasse E");
    case 4:
        return tr("Technik Klasse A");
    default:
        return tr("Prüfungsteil %1").arg(part);
    }
}

} // namespace hrt
