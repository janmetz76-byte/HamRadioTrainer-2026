// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"

#include <QDate>
#include <QDialog>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

class QButtonGroup;
class QDateEdit;
class QLabel;
class QRadioButton;

namespace hrt {

/// Start screen of the application, modelled on the configuration assistant of
/// the 2015 trainer.
///
/// The old program asked which exam is being prepared for and when it takes
/// place before it let anybody in. That is worth keeping - an application that
/// silently opens class N when the learner is sitting the A exam wastes their
/// time.
///
/// The learner picks one of the six ways to the certificate that
/// Amtsblatt-Verfügung 29/2024 knows: three examinations from scratch and
/// three ascents. The catalog follows from the class, and only the parts that
/// case requires are learned and examined.
///
/// The date is optional and only informational. It is shown as a countdown;
/// it does not change which questions are asked.
class StartupDialog : public QDialog
{
    Q_OBJECT

public:
    struct Result
    {
        QString combination;        ///< id of the chosen case, see Combination::id
        QString catalogPath;        ///< catalog of the target class
        QSet<int> selectedParts;    ///< indexes into the catalog's part list
        QDate examDate;             ///< invalid when none was given
        bool accepted = false;
    };

    StartupDialog(const QStringList &catalogPaths, QWidget *parent = nullptr);

    Result result() const { return m_result; }

    /// Selects the case with the given id, typically the one chosen last time.
    /// Unknown ids and cases without a catalog are ignored.
    void preselect(const QString &combinationId);

    /// Oeffentlich, damit der Test denselben Weg nimmt wie der Knopf.
    void accept() override;

    /// One way of coming to the examination, as Amtsblatt-Verfügung 29/2024
    /// puts it.
    struct Combination
    {
        QString id;          ///< stable key for the settings: "N", "N-E", ...
        QString label;       ///< shown at the radio button
        QString note;        ///< why the other parts are not needed
        QString targetClass; ///< class the certificate is applied for
        QVector<int> parts;  ///< indexes into the catalog's part list
        bool ascent = false; ///< an earlier certificate is credited
    };

    /// The parts needed for a full examination of the given class, without
    /// any earlier certificate being credited.
    static QVector<int> partsForClass(const QString &examClass);

    /// All six cases the regulation describes: three examinations from
    /// scratch and three ascents.
    ///
    /// 7.3 der Verfuegung laesst bereits bestandene Frageboegen weg: wer die
    /// Klasse N hat, muss fuer die Klasse E nur noch E beantworten, wer die
    /// Klasse N hat, fuer die Klasse A noch E und A, und wer die Klasse E
    /// hat, fuer die Klasse A nur noch A.
    static QVector<Combination> combinations();

    /// Readable name of a Prüfungsteil, by its index.
    static QString partName(int part);

private:
    struct CatalogInfo
    {
        QString path;
        QString title;
        QVector<int> questionsPerPart;
    };

    void updateSummary();
    int chosenIndex() const;

    Result m_result;
    QVector<Combination> m_combinations;
    QHash<QString, CatalogInfo> m_catalogs; ///< by exam class
    QButtonGroup *m_choices = nullptr;
    QVector<QRadioButton *> m_buttons;
    QLabel *m_summary = nullptr;
    QDateEdit *m_date = nullptr;
};

} // namespace hrt
