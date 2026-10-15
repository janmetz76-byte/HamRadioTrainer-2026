// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include "core/Catalog.h"
#include "core/CatalogValidator.h"
#include "exam/ExamSession.h"
#include "learning/ProgressStore.h"

#include <QMainWindow>
#include <QVector>

class QLabel;
class QPlainTextEdit;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace hrt {
class ExamWidget;
} // namespace hrt

namespace hrt {

/// Shell of HamRadioTrainer 2026. It opens plain XML question catalogs,
/// validates them and shows their structure in the catalog tab, and drives the
/// examination in the exam tab. Learning statistics build on top of this.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openCatalog(const QString &path);

private:
    void loadProgress();
    void saveProgress();
    void showResult(const ExamResult &result);
    void buildUi();
    void populateTree();
    void showQuestion(QTreeWidgetItem *item);
    void updateStatus();
    void showIssues(const QVector<CatalogIssue> &issues);

    Catalog m_catalog;
    QString m_catalogPath;
    QTabWidget *m_tabs = nullptr;
    ExamWidget *m_exam = nullptr;
    ProgressStore m_progressStore{ProgressStore(QString())};
    QTreeWidget *m_tree = nullptr;
    QPlainTextEdit *m_details = nullptr;
    QLabel *m_status = nullptr;
};

} // namespace hrt