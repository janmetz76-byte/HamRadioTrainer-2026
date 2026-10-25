// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include <QString>
#include <QVector>

namespace hrt {

/// One way of coming to the examination, as Amtsblatt-Verfügung 29/2024
/// puts it.
///
/// The cases live here and not in a dialog, because the desktop and the phone
/// interface offer the very same choice and must not drift apart.
struct ExamCombination
{
    QString id;          ///< stable key for the settings: "N", "N-E", ...
    QString label;       ///< shown at the choice
    QString note;        ///< why the other parts are not needed
    QString targetClass; ///< class the certificate is applied for
    QVector<int> parts;  ///< indexes into the catalog's part list
    bool ascent = false; ///< an earlier certificate is credited
};

/// The parts needed for a full examination of the given class, without any
/// earlier certificate being credited.
///
/// The parts are numbered alike in all three catalogs: 0 = V, 1 = B, 2 = N,
/// 3 = E, 4 = A.
QVector<int> partsForClass(const QString &examClass);

/// All six cases the regulation describes: three examinations from scratch and
/// three ascents.
///
/// 7.3 der Verfuegung laesst bereits bestandene Frageboegen weg: wer die
/// Klasse N hat, muss fuer die Klasse E nur noch E beantworten, wer die
/// Klasse N hat, fuer die Klasse A noch E und A, und wer die Klasse E hat,
/// fuer die Klasse A nur noch A.
QVector<ExamCombination> examCombinations();

/// The case with the given id, or an empty one when there is none.
ExamCombination examCombination(const QString &id);

/// Readable name of a Prüfungsteil, by its index.
QString examPartName(int part);

} // namespace hrt
