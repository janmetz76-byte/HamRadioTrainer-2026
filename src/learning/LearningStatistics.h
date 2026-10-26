// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "learning/Scheduler.h"

#include <QDate>
#include <QSet>
#include <QString>
#include <QVector>

namespace hrt {

/// Progress of one Prüfungsteil.
struct PartProgress
{
    QString partId;
    QString letter;
    QString title;

    int questions = 0;
    int answered = 0;   ///< questions seen at least once
    int mastered = 0;   ///< far along in the Leitner boxes
    int shaky = 0;      ///< answered but back in box 0
    int due = 0;        ///< repetitions due today, never seen ones excluded

    int timesAnswered = 0;
    int timesCorrect = 0;

    /// Correct answers in percent, 0 when nothing was answered yet.
    double accuracy() const
    {
        return timesAnswered > 0 ? 100.0 * double(timesCorrect) / double(timesAnswered) : 0.0;
    }

    /// Share of the part that has been answered at least once, in percent.
    double coverage() const
    {
        return questions > 0 ? 100.0 * double(answered) / double(questions) : 0.0;
    }
};

/// Where the learner stands with one catalog.
///
/// The numbers are meant to be read as an answer to "what should I do next":
/// "fresh" is what has never been seen, "shaky" is what has been answered
/// wrongly and therefore needs to come back, and "due" is what the Leitner
/// spacing repeats today. A question that is not mastered never stops being
/// due, only the top box finally leaves the rotation.
struct LearningStatistics
{
    int questions = 0;      ///< all drawable questions of the catalog
    int answered = 0;
    int fresh = 0;          ///< never answered
    int shaky = 0;          ///< answered, but back in box 0
    int mastered = 0;       ///< reached the last Leitner box
    /// Questions whose repetition is due today. Never seen questions are NOT
    /// counted here, they have their own figure "fresh": "due" only asks which
    /// repetitions the Leitner spacing offers today.
    int due = 0;
    /// Questions answered for the first time today.
    int newToday = 0;

    int timesAnswered = 0;
    int timesCorrect = 0;

    /// Number of answered questions per Leitner box, index 0..MaxBox.
    QVector<int> byBox;

    QVector<PartProgress> parts;

    double accuracy() const
    {
        return timesAnswered > 0 ? 100.0 * double(timesCorrect) / double(timesAnswered) : 0.0;
    }

    /// Share of answered questions in percent.
    double coverage() const
    {
        return questions > 0 ? 100.0 * double(answered) / double(questions) : 0.0;
    }

    /// How many questions are worth practising most: never seen ones and the
    /// shaky ones. This is what the study mode works through first.
    int priorityCount() const { return fresh + shaky; }
};

/// Computes the statistics of a catalog under the given learning progress.
///
/// A question counts as mastered once it reaches the second to last box, which
/// corresponds to a 16 day spacing: from there on it comes back rarely and only
/// to guard against forgetting.
///
/// With \a parts only those Prüfungsteile are counted, given as indexes into
/// the catalog's part list; see effectiveParts().
LearningStatistics computeStatistics(const Catalog &catalog, const Scheduler &scheduler,
                                     const QDate &today = QDate::currentDate(),
                                     const QSet<int> &parts = {});

/// The Prüfungsteile a selection covers in this catalog, as indexes.
///
/// A selection that names none of the catalog's parts - empty, or made for a
/// catalog with more parts than this one - stands for all of them. A mismatch
/// must never leave the learner with nothing to practise.
QSet<int> effectiveParts(const Catalog &catalog, const QSet<int> &selected);

} // namespace hrt