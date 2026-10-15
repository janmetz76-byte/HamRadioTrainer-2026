// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"

#include <QDialog>
#include <QSet>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;

namespace hrt {

/// Start screen of the application, modelled on the configuration assistant of
/// the 2015 trainer.
///
/// The old program asked three things before it let anybody in: which exam is
/// being prepared for, which subject areas are wanted, and when the exam takes
/// place. That is worth keeping - an application that silently opens class N
/// when the learner is sitting the A exam wastes their time.
///
/// The date is optional and only informational. It is shown as a countdown;
/// it does not change which questions are asked.
class StartupDialog : public QDialog
{
    Q_OBJECT

public:
    struct Result
    {
        QString catalogPath;        ///< empty when nothing suitable was found
        QSet<int> selectedParts;    ///< indexes into the catalog's part list
        QDate examDate;             ///< invalid when none was given
        bool accepted = false;
    };

    StartupDialog(const QStringList &catalogPaths, QWidget *parent = nullptr);

    Result result() const { return m_result; }

    /// Oeffentlich, damit der Test denselben Weg nimmt wie der Knopf.
    void accept() override;

    /// The part combinations the Amtsblatt-Verfügung 29/2024 allows.
    ///
    /// Class N is made up of the parts V, B and N; class E adds E; class A
    /// contains all five. Offering anything else would be a combination no
    /// examination consists of.
    static QVector<int> partsForClass(const QString &examClass);

    /// Every allowed combination, for the check boxes.
    static QStringList combinationLabels();

private:
    void updateEnabledParts();

    Result m_result;
    QComboBox *m_catalog = nullptr;
    QVector<QCheckBox *> m_partBoxes;
    QLabel *m_hint = nullptr;
    QDateEdit *m_date = nullptr;
    QVector<QSet<int>> m_combinations;
};

} // namespace hrt