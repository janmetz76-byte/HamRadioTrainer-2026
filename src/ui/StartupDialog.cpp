// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "StartupDialog.h"

#include "core/CatalogXml.h"

#include <QButtonGroup>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace hrt {

StartupDialog::StartupDialog(const QStringList &catalogPaths, QWidget *parent)
    : QDialog(parent)
    , m_combinations(combinations())
{
    setWindowTitle(tr("HamRadioTrainer 2026 – Vorbereitung"));

    // Je Klasse ein Katalog. Die Fragenzahlen je Teil zeigen, was die Wahl
    // fuer den Lernaufwand bedeutet.
    for (const QString &path : catalogPaths) {
        Catalog catalog;
        QString error;
        if (!CatalogXml::load(path, &catalog, &error))
            continue;
        const QString examClass = examClassToString(catalog.metadata.examClass);
        if (m_catalogs.contains(examClass))
            continue;
        CatalogInfo info;
        info.path = path;
        info.title = catalog.metadata.title;
        for (const Part &part : catalog.parts) {
            int questions = 0;
            for (const Section &section : part.sections)
                questions += section.activeQuestionCount();
            info.questionsPerPart.append(questions);
        }
        m_catalogs.insert(examClass, info);
    }

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(
        tr("Für welche Prüfung möchten Sie lernen? Gelernt und geprüft werden nur die "
           "Prüfungsteile, die dafür nötig sind."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_choices = new QButtonGroup(this);
    auto *fresh = new QGroupBox(tr("Prüfung"), this);
    auto *freshLayout = new QVBoxLayout(fresh);
    auto *ascent = new QGroupBox(tr("Aufstieg (Zusatzprüfung) nach bestandener Klasse"), this);
    auto *ascentLayout = new QVBoxLayout(ascent);

    for (int i = 0; i < m_combinations.size(); ++i) {
        const Combination &combination = m_combinations.at(i);
        auto *button = new QRadioButton(combination.label, combination.ascent ? ascent : fresh);
        const bool available = m_catalogs.contains(combination.targetClass);
        button->setEnabled(available);
        if (!available)
            button->setToolTip(tr("Kein Fragenkatalog der Klasse %1 gefunden.")
                                   .arg(combination.targetClass));
        m_choices->addButton(button, i);
        (combination.ascent ? ascentLayout : freshLayout)->addWidget(button);
        m_buttons.append(button);
    }
    layout->addWidget(fresh);
    layout->addWidget(ascent);

    // Was die Wahl bedeutet, steht unter den Knoepfen: welche Teile, wie viele
    // Fragen und warum der Rest entfaellt.
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    m_summary->setObjectName(QStringLiteral("auswahlUebersicht"));
    layout->addWidget(m_summary);

    auto *dateRow = new QHBoxLayout;
    dateRow->addWidget(new QLabel(tr("Prüfungstermin (freiwillig):"), this));
    m_date = new QDateEdit(this);
    m_date->setCalendarPopup(true);
    m_date->setDate(QDate::currentDate().addMonths(3));
    dateRow->addWidget(m_date);
    dateRow->addStretch(1);
    layout->addLayout(dateRow);

    auto *dateHint = new QLabel(
        tr("Der Termin wird nur angezeigt, zum Beispiel als Countdown. Er ändert nicht, "
           "welche Fragen gestellt werden."),
        this);
    dateHint->setWordWrap(true);
    dateHint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(dateHint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    // Ohne geladene Qt-Uebersetzung stehen die Standardknoepfe auf "OK" und
    // "Cancel" mitten in einem deutschen Fenster.
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Weiter"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &StartupDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_choices, &QButtonGroup::idToggled, this, [this](int, bool checked) {
        if (checked)
            updateSummary();
    });

    // Vorauswahl: die erste Pruefung, fuer die es einen Katalog gibt.
    for (QRadioButton *button : std::as_const(m_buttons)) {
        if (button->isEnabled()) {
            button->setChecked(true);
            break;
        }
    }
    updateSummary();
}

void StartupDialog::preselect(const QString &combinationId)
{
    for (int i = 0; i < m_combinations.size(); ++i) {
        if (m_combinations.at(i).id == combinationId && m_buttons.at(i)->isEnabled()) {
            m_buttons.at(i)->setChecked(true);
            return;
        }
    }
}

int StartupDialog::chosenIndex() const
{
    return m_choices->checkedId();
}

void StartupDialog::updateSummary()
{
    const int index = chosenIndex();
    if (index < 0) {
        m_summary->setText(m_catalogs.isEmpty()
                               ? tr("Es wurde kein Fragenkatalog gefunden, deshalb lässt sich "
                                    "keine Prüfung wählen.")
                               : tr("Bitte eine Prüfung auswählen."));
        return;
    }

    const Combination &combination = m_combinations.at(index);
    const CatalogInfo info = m_catalogs.value(combination.targetClass);
    QStringList lines;
    int total = 0;
    for (int part : combination.parts) {
        const int questions = info.questionsPerPart.value(part);
        total += questions;
        lines.append(tr("%1 (%2 Fragen)").arg(partName(part).toHtmlEscaped()).arg(questions));
    }

    QString text = tr("<b>Zu lernen:</b> %1 – zusammen %2 Fragen.")
                       .arg(lines.join(QStringLiteral(", ")))
                       .arg(total);
    if (!combination.note.isEmpty())
        text += QStringLiteral("<br>") + combination.note.toHtmlEscaped();
    m_summary->setText(text);
}

void StartupDialog::accept()
{
    const int index = chosenIndex();
    if (index < 0) {
        updateSummary();
        return;
    }

    const Combination &combination = m_combinations.at(index);
    m_result.combination = combination.id;
    m_result.catalogPath = m_catalogs.value(combination.targetClass).path;
    m_result.selectedParts = QSet<int>(combination.parts.begin(), combination.parts.end());
    m_result.examDate = m_date->date();
    m_result.accepted = true;
    QDialog::accept();
}

} // namespace hrt
