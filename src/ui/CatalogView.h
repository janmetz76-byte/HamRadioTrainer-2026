// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "learning/Scheduler.h"

#include <QHash>
#include <QWidget>

class QLabel;
class QSplitter;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace hrt {

/// Browse view of a catalog, modelled on the workspace of the 2015 trainer.
///
/// Three panes instead of a tree and a wall of text:
///   * the chapter tree on the left, carrying the number of questions
///   * the questions of the selected chapter in the middle, with the state of
///     each one - which is what the old trainer called "Letzte Abfrage" and
///     "Wiedervorlage"
///   * the question itself below, with its answers, the solution hint and the
///     learning progress
///
/// The point is that the learner can see what is still weak without starting
/// a round: a question's stage and its next due date are the two things that
/// decide whether it comes up again.
class CatalogView : public QWidget
{
    Q_OBJECT

public:
    explicit CatalogView(QWidget *parent = nullptr);

    void setCatalog(const Catalog *catalog, const QString &catalogPath);
    void setProgress(const QVector<QuestionState> &states);

    /// Selects a question by its id and shows it. Used to jump from a search
    /// result or from an external reference.
    bool showQuestion(const QString &questionId);

private:
    void buildTree();
    QVector<const Question *> questionsBelow(const Part &part) const;
    QVector<const Question *> questionsBelow(const Section &section) const;
    int partAvailable(const Part &part) const;
    void fillQuestionList(QTreeWidgetItem *item);
    void showSelectedQuestion();
    void showQuestionDetail(const Question &question);
    void clearDetail();

    const Catalog *m_catalog = nullptr;
    QString m_catalogPath;
    QHash<QString, QuestionState> m_states;
    QHash<QTreeWidgetItem *, QVector<const Question *>> m_sets;

    QTreeWidget *m_tree = nullptr;
    QTableWidget *m_questions = nullptr;
    QSplitter *m_right = nullptr;
    QWidget *m_detail = nullptr;
    QLabel *m_detailHeading = nullptr;
};

} // namespace hrt