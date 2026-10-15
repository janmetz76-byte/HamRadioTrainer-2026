// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "MainWindow.h"

#include "CatalogView.h"
#include "ValidationReport.h"

#include "core/CatalogValidator.h"
#include "core/CatalogLocator.h"
#include "core/CatalogXml.h"
#include "ExamWidget.h"
#include "LearningWidget.h"

#include <QAbstractItemView>
#include <QAction>
#include <QButtonGroup>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace hrt {
namespace {

enum Column {
    ColumnTitle = 0,
    ColumnDetail,
    ColumnKind,
};

enum ItemRole {
    QuestionRole = Qt::UserRole + 1,
};

/// Number of drawable questions of a Prüfungsteil, including its sections.
int partAvailable(const Part &part)
{
    int count = 0;
    for (const Section &section : part.sections)
        count += section.activeQuestionCount();
    return count;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
}

MainWindow::~MainWindow()
{
    // The last paper may not have been followed by another click, so the
    // progress is written once more on the way out.
    saveProgress();
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("HamRadioTrainer 2026"));
    resize(1100, 720);

    auto *openAction = new QAction(tr("&Katalog öffnen…"), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Fragenkatalog öffnen"), QString(),
            tr("Fragenkataloge (*.xml);;Alle Dateien (*)"));
        if (!path.isEmpty())
            openCatalog(path);
    });

    auto *reloadAction = new QAction(tr("A&ktualisieren"), this);
    reloadAction->setShortcut(QKeySequence::Refresh);
    connect(reloadAction, &QAction::triggered, this,
            [this] { if (!m_catalogPath.isEmpty()) openCatalog(m_catalogPath); });

    auto *resetProgressAction = new QAction(tr("Lern&fortschritt zurücksetzen"), this);
    connect(resetProgressAction, &QAction::triggered, this, [this] {
        if (m_catalogPath.isEmpty())
            return;
        const auto answer = QMessageBox::question(
            this, tr("Lernfortschritt zurücksetzen"),
            tr("Alle Lernstufen, Fälligkeiten und Trefferquoten dieses Katalogs werden "
               "gelöscht. Die Fragenkataloge selbst bleiben unverändert. Wirklich löschen?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        QString error;
        if (m_progressStore.clear(m_catalog, &error)) {
            m_exam->clearProgress();
            m_learning->clearProgress();
        m_catalogView->setProgress({});
            m_status->setText(tr("Lernfortschritt zurückgesetzt"));
        } else {
            QMessageBox::warning(this, tr("Zurücksetzen fehlgeschlagen"), error);
        }
    });

    auto *quitAction = new QAction(tr("&Beenden"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *fileMenu = menuBar()->addMenu(tr("&Datei"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(reloadAction);
    fileMenu->addSeparator();
    fileMenu->addAction(resetProgressAction);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Hilfe"));
    auto *aboutAction = new QAction(tr("&Über HamRadioTrainer 2026"), this);
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(
            this, tr("Über HamRadioTrainer 2026"),
            tr("<h3>HamRadioTrainer 2026</h3>"
               "<p>Neu aufgebaut von Jan DC6DX.</p>"
               "<p>Lizenz: GNU General Public License v3 oder später.</p>"
               "<p>Fragenkataloge liegen als unverschlüsseltes XML vor und können "
               "direkt bearbeitet werden.</p>"));
    });
    helpMenu->addAction(aboutAction);

    // Ansichtsleiste links, Arbeitsbereich rechts - so ist es im Original
    // von 2015 aufgebaut. Reiter waren fuer diese Anwendung zu gedrängt:
    // man konnte nie sehen, was es sonst gibt.
    auto *rail = new QWidget(this);
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(4, 4, 4, 4);
    railLayout->setSpacing(4);
    m_viewGroup = new QButtonGroup(rail);
    m_viewGroup->setExclusive(true);

    m_catalogView = new CatalogView(this);
    m_exam = new ExamWidget(this);
    m_learning = new LearningWidget(this);

    const QVector<QPair<QString, QWidget *>> views = {
        {tr("Katalogansicht"), m_catalogView},
        {tr("Fragen lernen"), m_learning},
        {tr("Prüfungssimulation"), m_exam},
    };

    for (int i = 0; i < views.size(); ++i) {
        auto *button = new QRadioButton(views.at(i).first, rail);
        button->setToolTip(views.at(i).first);
        m_viewGroup->addButton(button, i);
        if (i == 0)
            button->setChecked(true);
        railLayout->addWidget(button);
        m_views.append(views.at(i).second);
    }
    railLayout->addStretch(1);

    connect(m_viewGroup, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton *button) { selectView(m_viewGroup->id(button)); });

    auto *body = new QStackedWidget(this);
    for (QWidget *view : m_views)
        body->addWidget(view);
    m_viewStack = body;

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(rail);
    splitter->addWidget(body);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({190, 1100});
    setCentralWidget(splitter);

    connect(m_exam, &ExamWidget::progressChanged, this, [this] { saveProgress(); });
    connect(m_learning, &LearningWidget::progressChanged, this, [this] { saveProgress(); });
    connect(m_exam, &ExamWidget::examFinished, this, &MainWindow::showResult);

    m_status = new QLabel(tr("Kein Katalog geladen"), this);
    statusBar()->addWidget(m_status, 1);

    // Der Pruefbericht ist ein andockbares Panel und kein Dialog: die
    // mitgelieferten Kataloge erzeugen mehrere hundert Hinweise, die den
    // Start nicht unterbrechen duerfen.
    m_report = new ValidationReport(this);
    m_reportDock = new QDockWidget(tr("Prüfbericht"), this);
    m_reportDock->setObjectName(QStringLiteral("validationReport"));
    m_reportDock->setWidget(m_report);
    addDockWidget(Qt::BottomDockWidgetArea, m_reportDock);
    m_reportDock->hide();

    auto *reportAction = m_reportDock->toggleViewAction();
    reportAction->setText(tr("Prüf&bericht"));
    menuBar()->addAction(reportAction);
}

/// Reads the learning progress that belongs to this catalog. A progress file
/// that belongs to an earlier revision of the catalog is not applied, because
/// the questions may have been reworded or replaced by hand.
/// Shows the graded paper: the points of every Prüfungsteil against its own
/// pass mark, and which parts have to be repeated orally.
void MainWindow::showResult(const ExamResult &result)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Ergebnis der Prüfung"));
    dialog.resize(780, 480);

    auto *layout = new QVBoxLayout(&dialog);

    auto *headline = new QLabel(&dialog);
    headline->setWordWrap(true);
    if (result.passed) {
        headline->setText(tr("<b>Bestanden.</b> %1 von %2 Punkten.")
                              .arg(result.achievedPoints)
                              .arg(result.maxPoints));
    } else {
        headline->setText(tr("<b>Nicht bestanden.</b> %1 von %2 Punkten. Bestehen je "
                             "Prüfungsteil ab %3 Punkten.")
                              .arg(result.achievedPoints)
                              .arg(result.maxPoints)
                              .arg(result.parts.isEmpty() ? 19 : result.parts.first().passScore));
    }
    layout->addWidget(headline);

    if (!result.passed && !result.parts.isEmpty() && !result.failedParts().isEmpty()) {
        auto *hint = new QLabel(&dialog);
        hint->setWordWrap(true);
        hint->setText(tr("Nicht bestanden: Prüfungsteil %1. Dieser Teil ist mündlich "
                         "nachzuprüfen; die übrigen Teile müssen nicht wiederholt werden.")
                          .arg(result.failedParts().join(QStringLiteral(", "))));
        layout->addWidget(hint);
    }

    auto *table = new QTableWidget(&dialog);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({tr("Prüfungsteil"), tr("Punkte"), tr("Bestehen"),
                                      tr("falsch"), tr("offen")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (const PartResult &part : result.parts) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0,
                       new QTableWidgetItem(QStringLiteral("%1 — %2").arg(part.letter, part.title)));
        table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("%1 / %2")
                                                        .arg(part.achievedPoints)
                                                        .arg(part.maxPoints)));
        table->setItem(row, 2, new QTableWidgetItem(QString::number(part.passScore)));
        table->setItem(row, 3, new QTableWidgetItem(QString::number(part.wrongAnswers)));
        table->setItem(row, 4, new QTableWidgetItem(QString::number(part.unanswered)));
        if (!part.passed()) {
            for (int column = 0; column < table->columnCount(); ++column)
                table->item(row, column)->setBackground(QColor(0xff, 0xe0, 0xe0));
        }
    }
    table->resizeColumnsToContents();
    layout->addWidget(table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::loadProgress()
{
    m_progressStore = ProgressStore(ProgressStore::defaultPath(m_catalog));
    const ProgressLoad loaded = m_progressStore.load(m_catalog);

    if (!loaded.error.isEmpty()) {
        m_status->setText(tr("Fortschritt nicht geladen: %1").arg(loaded.error));
        return;
    }
    if (loaded.discardedDueToCatalogChange) {
        QMessageBox::information(
            this, tr("Fortschritt zurückgesetzt"),
            tr("Der Katalog wurde seit dem letzten Bearbeiten geändert. Der bisherige "
               "Lernfortschritt passt nicht mehr zu den Fragen und wurde verworfen. "
               "Der neue Fortschritt wird ab dem nächsten Training aufgebaut."));
        m_exam->clearProgress();
        m_exam->setProgress({});
        m_learning->clearProgress();
        m_catalogView->setProgress({});
        return;
    }
    if (loaded.droppedStates > 0) {
        m_status->setText(tr("%1 Lernstände zu entfernten Fragen wurden verworfen")
                              .arg(loaded.droppedStates));
    }
    m_exam->setProgress(loaded.states);
    m_learning->setProgress(loaded.states);
    m_catalogView->setProgress(loaded.states);
}

void MainWindow::saveProgress()
{
    if (m_catalogPath.isEmpty())
        return;
    const QVector<QuestionState> states = m_exam->scheduler().states();
    QString error;
    if (!m_progressStore.save(m_catalog, states, &error)) {
        m_status->setText(tr("Fortschritt nicht gespeichert: %1").arg(error));
        return;
    }
    // Die Fragenliste zeigt Lernstufe und Wiedervorlage; ohne diese Zeile
    // bliebe sie nach jeder Runde stehen.
    m_catalogView->setProgress(states);
}

void MainWindow::rememberCatalog(const QString &path)
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), path);
}

void MainWindow::openInitialCatalog()
{
    // The catalog of last use, as long as it is still there. A hand edited
    // catalog keeps its path, so the learner's own questions are not lost.
    QSettings settings;
    const QString last = settings.value(QStringLiteral("catalog/lastPath")).toString();
    if (!last.isEmpty() && QFileInfo::exists(last) && openCatalog(last))
        return;

    const CatalogLocator::Result located = CatalogLocator::locate();
    if (located.paths.isEmpty()) {
        m_status->setText(located.problem);
        return;
    }
    openCatalog(located.paths.first());
}

bool MainWindow::openCatalog(const QString &path)
{
    Catalog catalog;
    QString error;
    if (!CatalogXml::load(path, &catalog, &error)) {
        QMessageBox::critical(this, tr("Katalog konnte nicht geladen werden"), error);
        m_status->setText(tr("Fehler: %1").arg(error));
        return false;
    }

    QVector<CatalogIssue> issues;
    // Der Validator rechnet die Bildpfade relativ zu einem Verzeichnis, nicht
    // relativ zur Katalogdatei. Uebergibt man hier den Dateipfad, meldet er
    // jedes einzelne Bild als fehlend.
    const bool usable =
        CatalogValidator::validate(catalog, &issues, QFileInfo(path).absolutePath());

    m_catalog = catalog;
    m_catalogPath = path;
    rememberCatalog(path);
    m_catalogView->setCatalog(&m_catalog, path);
    m_exam->setCatalog(&m_catalog, path);
    m_learning->setCatalog(&m_catalog, path);
    loadProgress();
    updateStatus();

    if (!usable) {
        showIssues(issues);
        return false;
    }

    int warnings = 0;
    for (const CatalogIssue &issue : issues) {
        if (issue.severity == IssueSeverity::Warning)
            ++warnings;
    }
    if (warnings > 0)
        showIssues(issues);
    return true;
}

void MainWindow::selectView(int index)
{
    if (index >= 0 && index < m_views.size())
        m_viewStack->setCurrentIndex(index);
}


void MainWindow::updateStatus()
{
    if (m_catalogPath.isEmpty()) {
        m_status->setText(tr("Kein Katalog geladen"));
        return;
    }
    m_status->setText(tr("%1 — Klasse %2, Prüfung %3 — %4 Fragen gesamt, %5 verfügbar")
                          .arg(QFileInfo(m_catalogPath).fileName(),
                               examClassToString(m_catalog.metadata.examClass),
                               m_catalog.metadata.examId)
                          .arg(m_catalog.totalQuestionCount())
                          .arg(m_catalog.activeQuestionCount()));
}

void MainWindow::showIssues(const QVector<CatalogIssue> &issues)
{
    m_report->setIssues(issues);

    const int errors = m_report->errorCount();
    const int warnings = m_report->warningCount();

    if (errors > 0) {
        // Ohne diese Meldung merkt niemand, dass der Katalog unbrauchbar ist.
        QMessageBox box(this);
        box.setIcon(QMessageBox::Critical);
        box.setWindowTitle(tr("Katalog enthält Fehler"));
        box.setText(tr("%1 Fehler, %2 Hinweise.").arg(errors).arg(warnings));
        box.setInformativeText(
            tr("Der Katalog wird nicht zum Lernen oder Prüfen verwendet. Die vollständige "
               "Liste steht im Prüfbericht unten."));
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
        m_reportDock->show();
        return;
    }

    if (warnings > 0) {
        // Hinweise blockieren nicht. Der Zähler im Menü weist auf sie hin.
        m_reportDock->setWindowTitle(
            tr("Prüfbericht (%1)").arg(warnings == 1 ? tr("1 Hinweis")
                                                    : tr("%1 Hinweise").arg(warnings)));
    }
}

} // namespace hrt

#include "MainWindow.moc"