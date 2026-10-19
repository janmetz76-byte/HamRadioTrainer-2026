// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "StatisticsWidget.h"

#include "AppIcons.h"
#include "SectionBar.h"

#include "core/CatalogXml.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace hrt {
namespace {

/// Die Farbe einer Stufe, dieselben Toene wie in den Symbolen der Lernansicht.
///
/// Zwei Orte mit derselben Zahl muessen nicht dieselbe Farbe haben - hier aber
/// schon: die Verteilung in dieser Ansicht und die Stufenleiste dort zeigen
/// dieselbe Groesse, und ein Balken in einer anderen Farbe als das Symbol
/// daneben waere eine Frage, die niemand beantworten kann.
QColor stufenFarbe(int box)
{
    switch (qBound(0, box, Scheduler::MaxBox)) {
    case 0:
        return QColor(0x9a, 0x9a, 0x9a);
    case 1:
        return QColor(0xb3, 0x26, 0x1e);
    case 2:
        return QColor(0x8f, 0x47, 0x07);
    case 3:
        return QColor(0x77, 0x57, 0x00);
    case 4:
        return QColor(0x48, 0x65, 0x07);
    default:
        return QColor(0x25, 0x6b, 0x28);
    }
}

/// The same words the learning view uses for a stage, so that the two views
/// can be read together. "Stufe 3" genuegt hier - das Bild daneben sagt die
/// Stufe, und die Bedeutung steht in der Lernansicht.
QString stufenName(int box)
{
    if (box <= 0)
        return StatisticsWidget::tr("neu");
    if (box >= Scheduler::MaxBox)
        return StatisticsWidget::tr("gefestigt");
    return StatisticsWidget::tr("Stufe %1").arg(box);
}

} // namespace

StatisticsWidget::StatisticsWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    m_overall = new QLabel(this);
    m_overall->setWordWrap(true);
    layout->addWidget(m_overall);

    // --- Verteilung ueber die Stufen -------------------------------------
    layout->addWidget(new SectionBar(tr("Verteilung über die Lernstufen"), this));
    m_stages = new QTableWidget(Scheduler::MaxBox + 1, 3, this);
    m_stages->setObjectName(QStringLiteral("statistikStufen"));
    m_stages->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stages->setSelectionMode(QAbstractItemView::NoSelection);
    m_stages->verticalHeader()->setVisible(false);
    m_stages->horizontalHeader()->setVisible(false);
    m_stages->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_stages->verticalHeader()->setDefaultSectionSize(34);
    layout->addWidget(m_stages);

    // --- Je Pruefungsteil ------------------------------------------------
    layout->addWidget(new SectionBar(tr("Je Prüfungsteil"), this));
    m_parts = new QTableWidget(0, 6, this);
    m_parts->setObjectName(QStringLiteral("statistikTeile"));
    m_parts->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_parts->setSelectionMode(QAbstractItemView::NoSelection);
    m_parts->verticalHeader()->setVisible(false);
    m_parts->setHorizontalHeaderLabels({tr("Prüfungsteil"), tr("Fragen"), tr("bearbeitet"),
                                        tr("gefestigt"), tr("wackelig"),
                                        tr("Trefferquote")});
    m_parts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_parts->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_parts);

    layout->addStretch(1);
}

void StatisticsWidget::clear()
{
    m_overall->clear();
    m_stages->setRowCount(0);
    m_parts->setRowCount(0);
}

void StatisticsWidget::setCatalog(const Catalog *catalog, const QString &catalogPath)
{
    Q_UNUSED(catalogPath)
    m_catalog = catalog;
    refresh();
}

void StatisticsWidget::setProgress(const QVector<QuestionState> &states)
{
    m_scheduler = Scheduler(states);
    refresh();
}

void StatisticsWidget::refresh()
{
    if (!m_catalog) {
        clear();
        return;
    }

    m_statistics = computeStatistics(*m_catalog, m_scheduler);

    // Ohne Katalog steht hier nichts. Eine Nullzeile "0 von 0 Fragen" waere
    // eine Zahl, die nach einem Fehler aussieht.
    if (m_statistics.questions <= 0) {
        clear();
        return;
    }

    updateOverall();
    updateStages();
    updateParts();
}

void StatisticsWidget::updateOverall()
{
    const LearningStatistics &s = m_statistics;

    // Eine Zahl je Zeile waere die ausfuehrlichste Fassung und die am
    // schlechtesten lesbare. Der Satz nennt, was wichtig ist, und die Tabelle
    // darunter die Zahlen dazu.
    m_overall->setText(
        tr("%1 von %2 Fragen bearbeitet · %3 % bearbeitet · Trefferquote %4 % · "
           "%5 fällig · %6 neu · %7 wackelig · %8 gefestigt")
            .arg(s.answered)
            .arg(s.questions)
            .arg(s.coverage(), 0, 'f', 1)
            .arg(s.accuracy(), 0, 'f', 1)
            .arg(s.due)
            .arg(s.fresh)
            .arg(s.shaky)
            .arg(s.mastered));
    m_overall->setToolTip(
        tr("Eine Frage gilt ab der vorletzten Stufe als gefestigt; sie kommt "
           "dann nur noch selten und dient als Gegenprobe gegen das Vergessen."));
}

void StatisticsWidget::updateStages()
{
    const LearningStatistics &s = m_statistics;
    m_stages->setRowCount(Scheduler::MaxBox + 1);

    const int groesste = [&] {
        int max = 0;
        for (int box = 0; box < s.byBox.size(); ++box)
            max = qMax(max, s.byBox.at(box));
        return max;
    }();

    for (int box = 0; box <= Scheduler::MaxBox; ++box) {
        const int anzahl = box < s.byBox.size() ? s.byBox.at(box) : 0;

        auto *name = new QTableWidgetItem(stufenName(box));
        name->setIcon(AppIcons::get(QStringLiteral("stufe-%1").arg(qBound(0, box,
                                                                         Scheduler::MaxBox))));
        m_stages->setItem(box, 0, name);

        // Die Zahl steht rechts, der Balken in der Mitte: so sind die beiden
        // grossen Zahlen - die Zahl der Stufe und die Anzahl - auf einer Linie
        // und vergleichbar, ohne den Kopf zu bewegen.
        // Balken und Zahl in getrennten Spalten, nicht in einer: eine Zelle
        // kann entweder ein Widget oder einen Text tragen. Wer beides
        // versucht, sieht eines davon nicht.
        QWidget *traeger = new QWidget(m_stages);
        auto *layout = new QHBoxLayout(traeger);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *balkenWidget = new QProgressBar(traeger);
        balkenWidget->setTextVisible(false);
        balkenWidget->setRange(0, qMax(1, groesste));
        balkenWidget->setValue(anzahl);
        balkenWidget->setFixedHeight(14);
        QPalette balkenPalette = balkenWidget->palette();
        balkenPalette.setColor(QPalette::Highlight, stufenFarbe(box));
        balkenWidget->setPalette(balkenPalette);
        layout->addWidget(balkenWidget);
        m_stages->setCellWidget(box, 1, traeger);

        auto *zahl = new QTableWidgetItem(QString::number(anzahl));
        zahl->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_stages->setItem(box, 2, zahl);
    }

    m_stages->setColumnWidth(0, 150);
    m_stages->setColumnWidth(1, 240);
    m_stages->setColumnWidth(2, 80);

    // Die Tabelle bekam sonst nur so viel Platz, wie ihre Kopfzeile braucht,
    // und die letzte Stufe verschwand hinter einer Bildlaufleiste. Sechs
    // Zeilen sind kein Zustand, den man aufrollen sollte.
    m_stages->horizontalHeader()->setStretchLastSection(false);
    m_stages->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_stages->setFixedHeight(m_stages->rowCount() * m_stages->verticalHeader()->defaultSectionSize()
                             + m_stages->horizontalHeader()->height()
                             + 2 * m_stages->frameWidth());
}

void StatisticsWidget::updateParts()
{
    const QVector<PartProgress> &teile = m_statistics.parts;
    m_parts->setRowCount(teile.size());

    for (int row = 0; row < teile.size(); ++row) {
        const PartProgress &teil = teile.at(row);
        m_parts->setItem(row, 0, new QTableWidgetItem(
                                   QStringLiteral("%1 — %2").arg(teil.letter, teil.title)));
        m_parts->setItem(row, 1, new QTableWidgetItem(QString::number(teil.questions)));
        m_parts->setItem(row, 2, new QTableWidgetItem(QString::number(teil.answered)));
        m_parts->setItem(row, 3, new QTableWidgetItem(QString::number(teil.mastered)));
        m_parts->setItem(row, 4, new QTableWidgetItem(QString::number(teil.shaky)));
        m_parts->setItem(row, 5,
                         new QTableWidgetItem(QStringLiteral("%1 %").arg(teil.accuracy(), 0, 'f', 1)));
        for (int spalte = 1; spalte < 6; ++spalte)
            m_parts->item(row, spalte)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
}

} // namespace hrt

#include "StatisticsWidget.moc"