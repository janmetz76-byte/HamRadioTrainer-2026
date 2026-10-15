// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/CatalogValidator.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QComboBox;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace hrt {

/// Shows what the validator found in a catalog.
///
/// The three shipped catalogs produce a few hundred notes about the source
/// licence and about historic question data. Those are not a reason to stop
/// the learner, so they are listed in a dock the user opens when they care -
/// not in a modal dialog at every start.
///
/// A QMessageBox would have been the obvious choice and the wrong one: its
/// detail view cannot be resized and cannot be searched, which makes a long
/// report unusable.
class ValidationReport : public QWidget
{
    Q_OBJECT

public:
    explicit ValidationReport(QWidget *parent = nullptr);

    /// Replaces the contents of the report.
    void setIssues(const QVector<CatalogIssue> &issues);

    int errorCount() const { return m_errors; }
    int warningCount() const { return m_warnings; }

    /// "3 Fehler, 12 Hinweise", for menus and status bars.
    QString summary() const;

private:
    void applyFilter();
    void copyToClipboard();

    QLabel *m_summary = nullptr;
    QComboBox *m_filter = nullptr;
    QTreeWidget *m_list = nullptr;
    QToolButton *m_copy = nullptr;
    QVector<CatalogIssue> m_issues;
    int m_errors = 0;
    int m_warnings = 0;
};

} // namespace hrt