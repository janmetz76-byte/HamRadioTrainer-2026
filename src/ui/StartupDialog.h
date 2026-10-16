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
class QGroupBox;
class QVBoxLayout;
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

    /// One way of coming to the examination, as Amtsblatt-Verfügung 29/2024
    /// puts it.
    struct Combination
    {
        QString label;      ///< shown as a check box
        QString note;       ///< why the learner does not need the other parts
        QString targetClass;///< class the certificate is applied for
        QVector<int> parts; ///< indexes into the catalog's part list
        /// Was in der Zeile angezeigt wird. Leer heisst: alles aus parts.
        ///
        /// Der Aufstieg von N nach A prueft laut Verfuegung E und A. In der
        /// Zeile steht nur die Technik Klasse A, weil das die einzige ist,
        /// die auf dem Zertifikat steht; der Hinweis nennt E mit.
        QVector<int> shownParts;
    };

    /// The parts needed for a full examination of the given class, without
    /// any earlier certificate being credited.
    static QVector<int> partsForClass(const QString &examClass);

    /// All six cases the regulation describes: three examinations from
    /// scratch and three ascents.
    ///
    /// 7.3 der Verfuegung laesst bereits bestandene Frageboegen weg: wer die
    /// Klasse N hat, muss fuer die Klasse E nur noch E beantworten, wer die
    /// Klasse N hat, fuer die Klasse A nur noch E und A, und wer die Klasse E
    /// hat, fuer die Klasse A nur noch A. Ohne diese Aufstiegsfaelle
    /// beantwortet der Assistent mehr, als die Verfuegung vorsieht.
    static QVector<Combination> combinations();

    /// Only the combinations whose parts all exist in the given catalog.
    static QVector<Combination> combinationsFor(const Catalog &catalog);

private:
    void rebuildPartChoices();

    Result m_result;
    QComboBox *m_catalog = nullptr;
    QVector<QCheckBox *> m_partBoxes;
    QLabel *m_hint = nullptr;
    QDateEdit *m_date = nullptr;
    QVector<Combination> m_shown;
    QGroupBox *m_partsBox = nullptr;
    QVBoxLayout *m_partsLayout = nullptr;
};

} // namespace hrt