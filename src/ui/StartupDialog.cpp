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
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QList>
#include <QVBoxLayout>

namespace hrt {

namespace {

/// Ausgeschriebene Namen der Pruefungsteile.
///
/// "V + B + N" sagt einem Lernenden nichts, der die Veruegung nicht kennt.
/// Die Buchstaben stehen nur noch in den amtlichen Titeln der Kataloge, die
/// Anzeige hier spricht Klartext.
QString partName(int part)
{
    switch (part) {
    case 0:
        return QObject::tr("Vorschriften");
    case 1:
        return QObject::tr("Betriebstechnik");
    case 2:
        return QObject::tr("Technik Klasse N");
    case 3:
        return QObject::tr("Technik Klasse E");
    case 4:
        return QObject::tr("Technik Klasse A");
    default:
        return QObject::tr("Prüfungsteil %1").arg(part);
    }
}

QString partList(const QVector<int> &parts)
{
    QStringList names;
    for (int part : parts)
        names.append(partName(part));
    return names.join(QObject::tr(" und "));
}

} // namespace

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

QVector<StartupDialog::Combination> StartupDialog::combinations()
{
    // Reihenfolge: erst die Pruefungen von vorn, danach die Aufstiege. Das ist
    // die Reihenfolge, in der jemand sie erlebt.
    //
    // Die Parts sind ueber alle drei Kataloge hinweg gleich nummeriert:
    // 0 = V, 1 = B, 2 = N, 3 = E, 4 = A.
    const QVector<int> vbn = {0, 1, 2};
    const QVector<int> vbne = {0, 1, 2, 3};

    return {
        {tr("Klasse N — vollständige Prüfung"),
         tr("Alle Teile von vorn."), QStringLiteral("N"), vbn},
        {tr("Klasse E — vollständige Prüfung"),
         tr("Alle Teile von vorn."), QStringLiteral("E"), vbne},
        {tr("Klasse A — vollständige Prüfung"),
         tr("Alle Teile von vorn."), QStringLiteral("A"), {0, 1, 2, 3, 4}},
        {tr("Klasse N → E"), tr("Vorschriften, Betriebstechnik und Technik Klasse N wurden schon geprüft."),
         QStringLiteral("E"), {3}, {}},
        {tr("Klasse N → A"),
         tr("Vorschriften, Betriebstechnik und Technik Klasse N wurden schon geprüft. "
            "Zusätzlich zur Technik Klasse A wird die Technik Klasse E geprüft."),
         QStringLiteral("A"), {3, 4}, {4}},
        {tr("Klasse E → A"), tr("Vorschriften, Betriebstechnik sowie die Technik N und E wurden schon geprüft."),
         QStringLiteral("A"), {4}, {4}},
    };
}

QVector<StartupDialog::Combination> StartupDialog::combinationsFor(const Catalog &catalog)
{
    QVector<Combination> usable;
    for (const Combination &combination : combinations()) {
        bool present = true;
        for (int part : combination.parts) {
            if (part >= catalog.parts.size()) {
                present = false;
                break;
            }
        }
        if (present)
            usable.append(combination);
    }
    return usable;
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
    m_partsBox = partsBox;
    m_partsLayout = partsLayout;

    // Muss vor dem ersten rebuildPartChoices() existieren: die Auswahl
    // erklaert hier, wie viele Faelle der Katalog hergibt.
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
    connect(m_catalog, &QComboBox::currentIndexChanged, this, [this] { rebuildPartChoices(); });
    rebuildPartChoices();
}

void StartupDialog::rebuildPartChoices()
{
    for (QCheckBox *box : std::as_const(m_partBoxes)) {
        m_partsLayout->removeWidget(box);
        box->deleteLater();
    }
    m_partBoxes.clear();
    m_shown.clear();

    Catalog catalog;
    QString error;
    const QString path = m_catalog->currentData().toString();
    const bool loaded = !path.isEmpty() && CatalogXml::load(path, &catalog, &error);

    // Welche Faelle ueberhaupt moeglich sind, haengt am Katalog: Die Klasse N
    // enthaelt keinen Teil E, dort laesst sich also kein Aufstieg nach E
    // anbieten.
    m_shown = loaded ? combinationsFor(catalog) : QVector<Combination>();

    for (const Combination &combination : m_shown) {
        // Angezeigt wird, was auf dem Zertifikat steht - bei einem Aufstieg
        // also nur die letzte Stufe, auch wenn mehr geprueft wird.
        const QVector<int> &shown = combination.shownParts.isEmpty() ? combination.parts
                                                                     : combination.shownParts;
        auto *box = new QCheckBox(
            QStringLiteral("%1  —  %2").arg(combination.label, partList(shown)),
            m_partsBox);
        if (!combination.note.isEmpty())
            box->setToolTip(combination.note);
        m_partsLayout->addWidget(box);
        m_partBoxes.append(box);
    }

    // Vorauswahl: die vollstaendige Pruefung der Klasse des Katalogs.
    QString klassen;
    if (loaded)
        klassen = examClassToString(catalog.metadata.examClass);
    for (int i = 0; i < m_shown.size(); ++i) {
        // Ein Aufstieg traegt eine Erklaerung, eine vollstaendige Pruefung
        // nicht. Nur letztere soll vorgewaehlt sein.
        m_partBoxes.at(i)->setChecked(m_shown.at(i).targetClass == klassen
                                      && m_shown.at(i).note.isEmpty());
    }
    // Genau eine Auswahl muss immer stehen.
    bool any = false;
    for (QCheckBox *box : std::as_const(m_partBoxes))
        any = any || box->isChecked();
    if (!any && !m_partBoxes.isEmpty())
        m_partBoxes.first()->setChecked(true);

    m_hint->setText(loaded ? tr("%1 von %2 Prüfungen passen zu diesem Katalog. "
                                "Nach bestandener Klasse entfallen die schon geprüften Teile.")
                                 .arg(m_shown.size())
                                 .arg(combinations().size())
                           : tr("Ohne Katalog lässt sich keine Prüfung wählen."));
}

void StartupDialog::accept()
{
    int chosen = -1;
    for (int i = 0; i < m_partBoxes.size(); ++i) {
        if (m_partBoxes.at(i)->isChecked())
            chosen = i;
    }
    if (chosen < 0) {
        m_hint->setText(tr("Bitte eine Prüfung auswählen."));
        return;
    }

    const Combination &combination = m_shown.at(chosen);
    m_result.catalogPath = m_catalog->currentData().toString();
    m_result.selectedParts = QSet<int>(combination.parts.begin(), combination.parts.end());
    m_result.examDate = m_date->date();
    m_result.accepted = true;
    QDialog::accept();
}

} // namespace hrt

#include "StartupDialog.moc"