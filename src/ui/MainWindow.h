// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
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

#include <QDate>
#include <QMainWindow>
#include <QSet>
#include <QVector>

class QButtonGroup;
class QResizeEvent;
class QToolButton;
class QStackedWidget;
class QDockWidget;
class QRadioButton;
class QAction;
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace hrt {
class CatalogView;
class ExamWidget;
class LearningWidget;
class StatisticsWidget;
class ValidationReport;
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

    /// Die drei Ansichten der Ansichtsleiste, in ihrer Reihenfolge.
    enum View {
        CatalogViewIndex = 0,
        LearningViewIndex = 1,
        ExamViewIndex = 2,
    };
    static constexpr int kCatalogView = CatalogViewIndex;
    static constexpr int kLearningView = LearningViewIndex;
    static constexpr int kExamView = ExamViewIndex;

    bool openCatalog(const QString &path);

    /// Opens the catalog the learner used last, or the first one that was
    /// installed. A packaged program must not ask for a catalog on every
    /// start.
    void openInitialCatalog();
    void runStartupAssistant();
    void updateExamCountdown();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void arrangeRail();
    void fitRailIntoSplitter();
    void loadProgress();
    void saveProgress();
    void applySelectedParts(const QSet<int> &parts);
    /// Herkunft und Lizenz des geladenen Katalogs fuer den Hilfetext.
    QString catalogProvenance() const;
    void showResult(const ExamResult &result);
    void rememberCatalog(const QString &path);
    void buildUi();
    void selectView(int index);
    void updateStatus();
    /// Schaltet die Eintraege des Menüs "Prüfbericht" scharf oder blass.
    void updateReportActions();
    /// Schreibt Frage und Sollstand in den Bericht. Der Validator sieht nur,
    /// ob ein Katalog benutzbar ist; der Sollstand sagt, ob er noch der
    /// ausgelieferte ist.
    void refreshReport(const QVector<CatalogIssue> &validatorIssues);
    /// Fragt nach dem Archivverzeichnis und importiert daraus die Kataloge.
    void updateCatalog();
    /// Fills the validation report. Errors keep blocking with a modal dialog,
    /// because an unusable catalog must not be used by accident. Warnings do
    /// not: they are collected in the dock and merely announced.
    void showIssues(const QVector<CatalogIssue> &issues);

    Catalog m_catalog;
    QString m_catalogPath;
    ExamWidget *m_exam = nullptr;
    LearningWidget *m_learning = nullptr;
    StatisticsWidget *m_statistics = nullptr;
    ProgressStore m_progressStore{ProgressStore(QString())};
    CatalogView *m_catalogView = nullptr;
    QList<QWidget *> m_views;
    QButtonGroup *m_viewGroup = nullptr;
    /// Die Ansichtsleiste und ihre Teile, die sich je nach Breite umstellen.
    QWidget *m_rail = nullptr;
    QWidget *m_railHeader = nullptr;
    QWidget *m_railTitle = nullptr;
    QWidget *m_railGroupTitle = nullptr;
    QWidget *m_railButtons = nullptr;
    QToolButton *m_railToggle = nullptr;
    bool m_compact = false;
    QStackedWidget *m_viewStack = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_examDateLabel = nullptr;
    QDate m_examDate;
    QDockWidget *m_reportDock = nullptr;
    ValidationReport *m_report = nullptr;
    /// Der Wegweiser des Docks, im Menü "Prüfbericht" aufgehängt.
    QAction *m_showReportAction = nullptr;
    QAction *m_recheckAction = nullptr;
    QAction *m_copyReportAction = nullptr;
    /// Import der Kataloge aus dem Archiv der Bundesnetzagentur.
    QAction *m_updateAction = nullptr;
};

} // namespace hrt