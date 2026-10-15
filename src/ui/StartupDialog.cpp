// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "StartupDialog.h"

#include "core/CatalogLocator.h"
#include "core/CatalogXml.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

namespace hrt {

QVector<int> StartupDialog::partsForClass(const QString &examClass)
{
    // Amtsblatt-Verfügung 29/2024. Klasse N besteht aus den Teilen V, B und N,
    // Klasse E aus V, B, N und E, Klasse A aus allen fuenf.
    if (examClass == QStringLiteral("N"))
        return {0, 1, 2};
    if (examClass == QStringLiteral("E"))
        return {0, 1, 2, 3};
    return {0, 1, 2, 3, 4};
}

namespace {

/// The three examinations that exist, in the order they are offered.
const char *const kClasses[] = {"N", "E", "A"};

} // namespace

QStringList StartupDialog::combinationLabels()
{
    // Die Beschriftung wird aus derselben Liste gebaut wie die Auswahl. Sie
    // getrennt zu schreiben und dann die Klasse aus dem Text zu erraten war
    /// genau der Fehler, der daraus alle fuenf Teile machte.
    QStringList labels;
    for (const char *klass : kClasses) {
        QStringList teile;
        for (int part : partsForClass(QString::fromLatin1(klass))) {
            static const QString zeichen = QStringLiteral("VBNEA");
            teile.append(QString(zeichen.at(part)));
        }
        labels.append(QStringLiteral("%1  (Klasse %2)").arg(teile.join(QStringLiteral(" + ")),
                                                       QString::fromLatin1(klass)));
    }
    return labels;
}

StartupDialog::StartupDialog(const QStringList &catalogPaths, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("HamRadioTrainer 2026 – Vorbereitung"));
    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("Wofür möchten Sie lernen? Die Antwort entscheidet, welcher Katalog geöffnet "
           "und welche Prüfungsteile angeboten werden."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_catalog = new QComboBox(this);
    for (const QString &path : catalogPaths) {
        Catalog catalog;
        QString error;
        QString label = QFileInfo(path).fileName();
        // Der Titel des Katalogs nennt die Klasse bereits; ein zweites Mal
        // sieht doppelt aus.
        if (CatalogXml::load(path, &catalog, &error) && !catalog.metadata.title.isEmpty())
            label = catalog.metadata.title;
        m_catalog->addItem(label, path);
    }
    if (m_catalog->count() == 0) {
        m_catalog->addItem(tr("Kein Katalog gefunden"), QString());
        m_catalog->setEnabled(false);
    }
    layout->addWidget(new QLabel(tr("Prüfung:"), this));
    layout->addWidget(m_catalog);

    auto *partsBox = new QGroupBox(tr("Prüfungsteile"), this);
    auto *partsLayout = new QVBoxLayout(partsBox);

    // Die Originalprogramm zeigte hier eine Liste der erlaubten Kombinationen
    // (Klasse E, Erweiterungsprüfung E nach A, Klasse A). Das entspricht genau
    // den Teilemengen der Verfügung.
    for (int i = 0; i < combinationLabels().size(); ++i) {
        QSet<int> combination;
        for (int part : partsForClass(QString::fromLatin1(kClasses[i])))
            combination.insert(part);
        m_combinations.append(combination);

        auto *box = new QCheckBox(combinationLabels().at(i), partsBox);
        partsLayout->addWidget(box);
        m_partBoxes.append(box);
    }

    m_hint = new QLabel(partsBox);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    partsLayout->addWidget(m_hint);

    layout->addWidget(partsBox);

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

    // Vorauswahl: die Teile der gewaehlten Klasse.
    connect(m_catalog, &QComboBox::currentIndexChanged, this, [this] { updateEnabledParts(); });
    updateEnabledParts();
}

void StartupDialog::updateEnabledParts()
{
    // Die Klasse ergibt sich aus dem Katalog; stehen mehrere zur Wahl, richtet
    // sich die Vorauswahl nach dem ersten.
    int wanted = -1;
    const QString path = m_catalog->currentData().toString();
    if (!path.isEmpty()) {
        Catalog catalog;
        QString error;
        if (CatalogXml::load(path, &catalog, &error)) {
            const QString klassen = examClassToString(catalog.metadata.examClass);
            wanted = klassen == QStringLiteral("N") ? 0
                     : klassen == QStringLiteral("E") ? 1
                                                     : 2;
        }
    }

    for (int i = 0; i < m_partBoxes.size(); ++i) {
        m_partBoxes.at(i)->setChecked(i == wanted);
        m_partBoxes.at(i)->setEnabled(!path.isEmpty());
    }
    m_hint->setText(path.isEmpty()
                        ? tr("Ohne Katalog lässt sich keine Prüfung wählen.")
                        : tr("Wählbar sind nur die Prüfungsteile, aus denen die gewählte "
                             "Prüfung besteht."));
}

void StartupDialog::accept()
{
    QSet<int> parts;
    for (int i = 0; i < m_partBoxes.size(); ++i) {
        if (m_partBoxes.at(i)->isChecked())
            parts = m_combinations.at(i);
    }
    if (parts.isEmpty()) {
        m_hint->setText(tr("Bitte eine Prüfung auswählen."));
        return;
    }

    m_result.catalogPath = m_catalog->currentData().toString();
    m_result.selectedParts = parts;
    m_result.examDate = m_date->date();
    m_result.accepted = true;
    QDialog::accept();
}

} // namespace hrt

#include "StartupDialog.moc"