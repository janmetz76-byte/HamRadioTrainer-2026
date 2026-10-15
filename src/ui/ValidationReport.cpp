// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ValidationReport.h"

#include <QClipboard>
#include <QComboBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace hrt {
namespace {

enum FilterRole { kSeverityRole = Qt::UserRole + 1 };

} // namespace

ValidationReport::ValidationReport(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    auto *controls = new QHBoxLayout;
    m_filter = new QComboBox(this);
    m_filter->addItem(tr("Alle Meldungen"), 0);
    m_filter->addItem(tr("Nur Fehler"), 1);
    m_filter->addItem(tr("Nur Hinweise"), 2);
    controls->addWidget(new QLabel(tr("Zeige:"), this));
    controls->addWidget(m_filter);
    controls->addStretch(1);

    m_copy = new QToolButton(this);
    m_copy->setText(tr("Alles kopieren"));
    m_copy->setToolTip(tr("Den vollständigen Prüfbericht in die Zwischenablage kopieren"));
    controls->addWidget(m_copy);
    layout->addLayout(controls);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({tr("Art"), tr("Ort"), tr("Meldung")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAlternatingRowColors(true);
    m_list->setWordWrap(true);
    m_list->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_list->setSortingEnabled(true);
    layout->addWidget(m_list, 1);

    connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { applyFilter(); });
    connect(m_copy, &QToolButton::clicked, this, &ValidationReport::copyToClipboard);
}

void ValidationReport::setIssues(const QVector<CatalogIssue> &issues)
{
    m_issues = issues;
    m_errors = 0;
    m_warnings = 0;
    for (const CatalogIssue &issue : issues) {
        if (issue.severity == IssueSeverity::Error)
            ++m_errors;
        else
            ++m_warnings;
    }
    m_summary->setText(summary());
    applyFilter();
}

QString ValidationReport::summary() const
{
    if (m_issues.isEmpty())
        return tr("Der Katalog enthält keine Beanstandungen.");
    return tr("%1 Fehler, %2 Hinweise. Ein Fehler macht den Katalog unbrauchbar; "
              "ein Hinweis nicht.")
        .arg(m_errors)
        .arg(m_warnings);
}

void ValidationReport::applyFilter()
{
    const int wanted = m_filter->currentData().toInt();

    m_list->setUpdatesEnabled(false);
    m_list->clear();
    for (const CatalogIssue &issue : std::as_const(m_issues)) {
        const bool isError = issue.severity == IssueSeverity::Error;
        if (wanted == 1 && !isError)
            continue;
        if (wanted == 2 && isError)
            continue;

        auto *row = new QTreeWidgetItem(m_list);
        row->setText(0, isError ? tr("Fehler") : tr("Hinweis"));
        row->setText(1, issue.location);
        row->setText(2, issue.message);
        row->setData(0, kSeverityRole, isError ? 0 : 1);
        row->setToolTip(2, issue.message);
    }
    m_list->setUpdatesEnabled(true);
}

void ValidationReport::copyToClipboard()
{
    QString text;
    for (const CatalogIssue &issue : std::as_const(m_issues)) {
        text += (issue.severity == IssueSeverity::Error ? tr("FEHLER: ") : tr("HINWEIS: "));
        text += issue.location;
        text += QStringLiteral(" — ");
        text += issue.message;
        text += QLatin1Char('\n');
    }
    QGuiApplication::clipboard()->setText(text);
}

} // namespace hrt

#include "ValidationReport.moc"