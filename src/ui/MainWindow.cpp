// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "MainWindow.h"

#include "core/CatalogValidator.h"
#include "core/CatalogXml.h"
#include "ExamWidget.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
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
            tr("Alle Leitner-Ordner, Fälligkeiten und Trefferquoten dieses Katalogs werden "
               "gelöscht. Die Fragenkataloge selbst bleiben unverändert. Wirklich löschen?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        QString error;
        if (m_progressStore.clear(m_catalog, &error)) {
            m_exam->clearProgress();
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

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels({tr("Titel"), tr("Detail"), tr("Art")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
            [this] { showQuestion(m_tree->currentItem()); });

    m_details = new QPlainTextEdit(this);
    m_details->setReadOnly(true);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_tree);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(1, 1);

    m_exam = new ExamWidget(this);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(splitter, tr("Katalog"));
    m_tabs->addTab(m_exam, tr("Prüfung"));
    setCentralWidget(m_tabs);

    connect(m_exam, &ExamWidget::progressChanged, this, [this] { saveProgress(); });
    connect(m_exam, &ExamWidget::examFinished, this, &MainWindow::showResult);

    m_status = new QLabel(tr("Kein Katalog geladen"), this);
    statusBar()->addWidget(m_status, 1);
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
        return;
    }
    if (loaded.droppedStates > 0) {
        m_status->setText(tr("%1 Lernstände zu entfernten Fragen wurden verworfen")
                              .arg(loaded.droppedStates));
    }
    m_exam->setProgress(loaded.states);
}

void MainWindow::saveProgress()
{
    if (m_catalogPath.isEmpty())
        return;
    QString error;
    if (!m_progressStore.save(m_catalog, m_exam->scheduler().states(), &error))
        m_status->setText(tr("Fortschritt nicht gespeichert: %1").arg(error));
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
    const bool usable = CatalogValidator::validate(catalog, &issues, path);

    m_catalog = catalog;
    m_catalogPath = path;
    m_exam->setCatalog(&m_catalog, path);
    loadProgress();
    populateTree();
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

void MainWindow::populateTree()
{
    m_tree->clear();

    for (const Part &part : m_catalog.parts) {
        auto *partItem = new QTreeWidgetItem(m_tree);
        partItem->setText(0, part.title);
        partItem->setText(1, part.id);
        partItem->setText(
            ColumnDetail,
            tr("Prüfungsteil, %1 von %2 Fragen in der Prüfung, %3 im Katalog")
                .arg(part.selection.questions)
                .arg(part.selection.questions)
                .arg(partAvailable(part)));
        partItem->setText(ColumnKind,
                          tr("bestanden ab %1 von %2 Punkten, %3 Fehlerpunkte zulässig, %4 min")
                              .arg(part.selection.passScore.value_or(0))
                              .arg(part.selection.questions)
                              .arg(part.selection.errorPoints.value_or(0))
                              .arg(part.selection.minutes.value_or(0)));

        for (const Section &section : part.sections) {
            auto *sectionItem = new QTreeWidgetItem(partItem);
            sectionItem->setText(0, section.title);
            sectionItem->setText(1, section.id);
            sectionItem->setText(2, tr("Abschnitt, %1 Fragen").arg(section.questions().size()));

            for (const Chapter &chapter : section.chapters) {
                auto *chapterItem = new QTreeWidgetItem(sectionItem);
                chapterItem->setText(0, chapter.title);
                chapterItem->setText(1, chapter.id);
                chapterItem->setText(2, tr("Kapitel"));

                for (const Question &question : chapter.questions) {
                    auto *questionItem = new QTreeWidgetItem(chapterItem);
                    questionItem->setText(0, question.text);
                    questionItem->setText(1, question.id);
                    questionItem->setText(ColumnKind,
                                          tr("%1 · %2 Punkte%3%4")
                                              .arg(question.isMultipleChoice()
                                                       ? tr("Mehrfachauswahl")
                                                       : tr("Einfachauswahl"))
                                              .arg(question.points)
                                              .arg(question.locked ? tr(" · gesperrt") : QString())
                                              .arg(question.isRetired() ? tr(" · zurückgezogen")
                                                                         : QString()));
                    questionItem->setData(0, QuestionRole, true);
                    if (question.locked || question.isRetired())
                        questionItem->setForeground(0, Qt::gray);
                }
            }
        }
    }

    m_tree->expandToDepth(1);
}

void MainWindow::showQuestion(QTreeWidgetItem *item)
{
    if (!item || !item->data(0, QuestionRole).toBool()) {
        m_details->clear();
        return;
    }

    const QString id = item->text(1);
    const Question *question = nullptr;
    for (Question *candidate : m_catalog.questions()) {
        if (candidate->id == id) {
            question = candidate;
            break;
        }
    }
    if (!question)
        return;

    QString text;
    text += tr("Kennung: %1\n").arg(question->id);
    text += tr("Fehlerpunkte: %1\n").arg(question->points);
    text += tr("Aufnahme: %1\n").arg(question->history.addedOn.toString(Qt::ISODate));
    if (question->history.changedOn.isValid())
        text += tr("Geändert: %1\n").arg(question->history.changedOn.toString(Qt::ISODate));
    if (question->history.retiredOn.isValid())
        text += tr("Zurückgezogen: %1\n").arg(question->history.retiredOn.toString(Qt::ISODate));
    text += QLatin1Char('\n');
    text += question->text + QStringLiteral("\n\n");

    for (const Answer &answer : question->answers) {
        text += (answer.correct ? QStringLiteral("[x] ") : QStringLiteral("[ ] "));
        text += answer.text + QLatin1Char('\n');
    }

    if (!question->explanation.isEmpty())
        text += QStringLiteral("\n") + tr("Erklärung: %1").arg(question->explanation);
    if (!question->image.isEmpty())
        text += QStringLiteral("\n") + tr("Bild: %1").arg(question->image);

    m_details->setPlainText(text);
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
    QString text;
    int errors = 0;
    int warnings = 0;
    for (const CatalogIssue &issue : issues) {
        if (issue.severity == IssueSeverity::Error)
            ++errors;
        else
            ++warnings;
        text += (issue.severity == IssueSeverity::Error ? tr("FEHLER: ") : tr("HINWEIS: "));
        text += issue.location;
        text += QStringLiteral(" — ");
        text += issue.message;
        text += QLatin1Char('\n');
    }

    QMessageBox box(this);
    box.setIcon(errors > 0 ? QMessageBox::Critical : QMessageBox::Information);
    box.setWindowTitle(errors > 0 ? tr("Katalog enthält Fehler") : tr("Katalog enthält Hinweise"));
    box.setText(tr("%1 Fehler, %2 Hinweise").arg(errors).arg(warnings));
    box.setDetailedText(text);
    box.exec();
}

} // namespace hrt

#include "MainWindow.moc"