// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include "core/Catalog.h"
#include "learning/Scheduler.h"

#include <QRandomGenerator>
#include <QString>
#include <QVector>

namespace hrt {

/// One question of a generated paper, together with the order in which its
/// answers are presented.
///
/// The catalog stores the answer order of the source, where the correct answer
/// is always the first one. That order must never reach the learner: the
/// original trainer mixed the answers on every run, and a fixed order would
/// make "always click the first entry" a winning strategy. The permutation is
/// therefore generated per question and can be reproduced from the paper seed.
struct PaperQuestion
{
    QString questionId;
    /// Indices into Question::answers, in presentation order.
    QVector<int> answerOrder;
};

/// One Prüfungsteil of a generated paper, with the limits that apply to it.
struct PaperPartSummary
{
    QString partId;
    /// Prüfungsteil letter, e.g. "V", "B", "N", "E", "A".
    QString letter;
    QString title;
    int questionCount = 0;
    int maxPoints = 0;
    /// Points required in this Prüfungsteil, e.g. 19 of 25.
    int passScore = 0;
    /// Wrong answers still tolerated in this Prüfungsteil, e.g. 6 of 25.
    int errorPointBudget = 0;
    int minutes = 0;
};

/// A generated exam or training paper.
struct ExamPaper
{
    quint32 seed = 0;
    QVector<PaperQuestion> questions;
    int totalPoints = 0;

    /// Per Prüfungsteil limits, filled in exam mode. Empty for training papers.
    QVector<PaperPartSummary> parts;
    /// Sum of the time limits of all Prüfungsteile.
    int totalMinutes = 0;
    /// Points required overall, e.g. 76 of 100 for a class E certificate.
    int passScore = 0;
    /// Wrong answers tolerated overall, e.g. 18 of 100 for class E.
    int errorPointBudget = 0;

    /// True when every Prüfungsteil of the catalog is represented with its full
    /// official question count, i.e. the paper reproduces a real examination.
    bool isFullExam() const;
};

/// Builds papers from a catalog and the learner's progress.
class PaperBuilder
{
public:
    /// How the questions of one paper are chosen.
    struct Options
    {
        /// 0 draws from the whole catalog, regardless of Prüfungsteil.
        int questionCount = 0;
        /// Questions per Prüfungsteil index. When set, it overrides
        /// "questionCount" and the parts are filled in this order.
        QVector<int> questionsPerPart;
        /// When non-empty the total penalty of the paper is steered towards
        /// this budget. 0 means "no budget".
        int errorPointBudget = 0;
        /// Ignore the due date and the recency penalty of the scheduler.
        bool reviewMode = false;
        /// Reproduce a real examination: draw the official question count of
        /// every Prüfungsteil of the catalog and apply its limits.
        bool examMode = false;
    };

    /// Draws a paper. Locked and retired questions are never drawn.
    ///
    /// Selection is weighted: questions the learner knows well are asked less
    /// often. The weighting comes from Scheduler::weightFor().
    static ExamPaper build(const Catalog &catalog,
                           const Scheduler &scheduler,
                           const Options &options,
                           quint32 seed);

    /// Shuffles the answers of one question deterministically from the seed.
    static QVector<int> shuffledAnswerOrder(const Question &question, quint32 seed,
                                            const QString &questionId);

    /// Builds the options that reproduce the official examination of a
    /// catalog: the question count of every Prüfungsteil as declared in the
    /// catalog. Parts without a declared count are skipped.
    static Options examOptions(const Catalog &catalog);

    /// Letter of a Prüfungsteil, taken from the last segment of its id
    /// ("DE-E-teil4-e" -> "E"). Empty when the id does not end in one.
    static QString partLetter(const Part &part);

    /// Parses an "E,A,B,D" pattern into answer indices of the given question.
    static QVector<int> parseAnswerOrder(const Question &question, const QString &pattern);
};

} // namespace hrt