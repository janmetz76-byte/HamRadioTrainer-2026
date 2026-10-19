// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include "core/Catalog.h"
#include "exam/PaperBuilder.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace hrt {

/// Result of one Prüfungsteil.
///
/// The pass mark applies per Prüfungsteil, not to the sum: Amtsblatt-Verfügung
/// 29/2024 requires 19 of 25 points in every part, and only if every part is
/// passed is the certificate awarded. Failing a single part triggers the
/// mündliche Nachprüfung in exactly that part.
struct PartResult
{
    QString partId;
    QString letter;
    QString title;
    int questionCount = 0;
    int maxPoints = 0;
    int achievedPoints = 0;
    int passScore = 0;
    int errorPointBudget = 0;
    int wrongAnswers = 0;
    int unanswered = 0;

    bool passed() const { return questionCount > 0 && achievedPoints >= passScore; }

    /// Questions to repeat in the mündliche Nachprüfung, in paper order.
    QStringList failedQuestionIds;
};

/// Result of a whole examination.
struct ExamResult
{
    QVector<PartResult> parts;

    int totalQuestions = 0;
    int maxPoints = 0;
    int achievedPoints = 0;
    int passScore = 0;
    int errorPointBudget = 0;
    int wrongAnswers = 0;
    int unanswered = 0;

    /// True only if every Prüfungsteil reached its own pass mark.
    bool passed = false;

    /// The Prüfungsteile that have to be repeated orally, in catalog order.
    QStringList failedParts() const;
    /// Points still missing in the part that is closest to failing.
    int weakestShortfall() const;
};

/// One exam paper in progress.
///
/// The class knows nothing about widgets: it holds the paper, remembers which
/// answer the learner picked for which question and can grade the attempt. The
/// user interface only reads from it and forwards clicks. Keeping the grading
/// here makes the pass marks of the Verordnung testable without a display.
class ExamSession
{
public:
    ExamSession(const Catalog &catalog, const ExamPaper &paper);

    int count() const { return m_paper.questions.size(); }
    bool isEmpty() const { return count() == 0; }
    quint32 seed() const { return m_paper.seed; }
    const ExamPaper &paper() const { return m_paper; }

    /// Question shown at the given position of the paper.
    const Question *questionAt(int index) const;

    /// Presentation order of the answers of the question at the given
    /// position: answerOrder.at(position) is the index into
    /// Question::answers shown at that position.
    const QVector<int> &answerOrderAt(int index) const;

    /// Prüfungsteil letter and title of the question at the given position.
    QString partLetterAt(int index) const;
    QString partTitleAt(int index) const;

    /// Records the answer the learner picked, given as the position of the
    /// displayed answer. Re-picking replaces the previous answer.
    void answer(int index, int displayedPosition);

    /// Displayed position of the recorded answer, or -1 if unanswered.
    int selectedPosition(int index) const;
    bool isAnswered(int index) const;
    int answeredCount() const;

    /// Number of questions of one Prüfungsteil letter in this paper.
    int countOfPart(const QString &letter) const;
    int answeredCountOfPart(const QString &letter) const;

    /// Index of the first unanswered question at or after "from", or -1 when
    /// everything is answered. Returns "from" when it is still unanswered, so
    /// a simple forward loop visits everything exactly once.
    int nextUnanswered(int from) const;

    /// Grades the attempt. Questions left unanswered count as wrong, because
    /// the Veriftung grants points only for correct answers.
    ExamResult evaluate() const;

    /// Correct answer of a question as the displayed position, or -1 when the
    /// question has no correct answer at all.
    int correctPositionAt(int index) const;

private:
    const Catalog &m_catalog;
    ExamPaper m_paper;
    QHash<QString, const Question *> m_byId;
    QVector<int> m_selected;      ///< displayed position, -1 when unanswered
    QVector<PartResult> m_partMeta; ///< limits per part, in paper order
};

} // namespace hrt