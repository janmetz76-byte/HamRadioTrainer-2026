// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include <QDate>
#include <QString>
#include <QVector>

namespace hrt {

/// How sure the learner felt when answering. Stored alongside the objective
/// result, because a question answered correctly while feeling very unsure is
/// exactly the one that has to come back.
enum class Confidence {
    Unknown = -1,  ///< never answered, or answered before this feature existed
    Guessing = 0,
    Unsure = 1,
    FairlySure = 2,
    Certain = 3,
};

QString confidenceToString(Confidence value);

/// Persistent learning state of one question.
///
/// Two independent signals drive the selection:
///   * the Leitner box, raised on correct answers and lowered on wrong ones
///   * the self reported confidence of the learner
///
/// A question that is answered correctly but felt uncertain is treated as less
/// known than one answered correctly with certainty. That is what keeps the
/// trainer asking the shaky questions more often.
struct QuestionState
{
    QString questionId;
    int box = 0;                ///< Leitner box, 0 means never answered correctly
    int timesAnswered = 0;
    int timesCorrect = 0;
    int consecutiveCorrect = 0;
    QDate lastSeen;             ///< invalid means never seen
    QDate dueOn;                ///< invalid means due immediately
    Confidence confidence = Confidence::Unknown;

    bool isNew() const { return timesAnswered == 0; }
};

/// Result of one answered question.
struct AnswerOutcome
{
    QString questionId;
    bool correct = false;
    Confidence confidence = Confidence::Guessing;
};

/// Keeps and advances the learning states of a catalog.
///
/// The rules are deliberately simple and documented rather than tuned:
///   * correct, certain -> box + 3 (capped)
///   * correct, unsure or without a self report -> box + 1
///   * correct, guessed -> box unchanged, due again tomorrow: a guessed hit is
///     counted in the hit rate but is not knowledge
///   * incorrect -> box = max(0, box - 2), consecutiveCorrect = 0
class Scheduler
{
public:
    /// Number of Leitner boxes.
    static constexpr int MaxBox = 5;

    explicit Scheduler(QVector<QuestionState> states = {});

    const QVector<QuestionState> &states() const { return m_states; }

    QuestionState stateFor(const QString &questionId) const;

    /// Records an answered question and returns the new state.
    const QuestionState &record(const AnswerOutcome &outcome);

    /// Relative likelihood that a question is asked again. Small means "rarely".
    ///
    /// Never asked questions get a high weight so that the whole pool is
    /// covered once. Confident questions in a high box get a very small weight.
    double weightFor(const QuestionState &state) const;

    /// Questions sorted by descending weight. Used by the learner view to offer
    /// the most useful question first.
    QVector<QString> questionsByPriority(const QStringList &candidateIds) const;

    /// Number of questions per Leitner box, for the statistics view.
    QVector<int> distribution() const;

    /// Forgets all progress.
    void reset();

private:
    QuestionState *findOrCreate(const QString &questionId);

    QVector<QuestionState> m_states;
};

} // namespace hrt