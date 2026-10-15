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

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>

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

MainWindow::~MainWindow() = default;

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

    auto *quitAction = new QAction(tr("&Beenden"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *fileMenu = menuBar()->addMenu(tr("&Datei"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(reloadAction);
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
    setCentralWidget(splitter);

    m_status = new QLabel(tr("Kein Katalog geladen"), this);
    statusBar()->addWidget(m_status, 1);
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
        partItem->setText(2, tr("Prüfungsteil"));

        for (const Section &section : part.sections) {
            auto *sectionItem = new QTreeWidgetItem(partItem);
            sectionItem->setText(0, section.title);
            sectionItem->setText(1, section.id);
            sectionItem->setText(2, tr("Abschnitt, %1 Fragen").arg(section.questions().size()));
            sectionItem->setText(
                1, tr("%1 · %2 Fragen Prüfungsteil, %3 verfügbar")
                        .arg(section.id)
                        .arg(part.selection.questions)
                        .arg(part.selection.questions > 0 ? partAvailable(part) : 0));

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