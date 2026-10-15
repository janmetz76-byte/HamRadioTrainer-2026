// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "learning/Scheduler.h"

#include <QStringList>

namespace hrt {

/// What a practice round should work through.
struct StudyOptions
{
    /// 0 means "no limit".
    int maxQuestions = 0;
    /// Practise only what is worth repeating today. Questions that were never
    /// seen count as worth it, otherwise a fresh catalog would offer nothing
    /// to do at all; "includeFresh" turns that off.
    bool onlyDue = false;
    /// Practise only questions that were answered wrongly before.
    bool onlyShaky = false;
    /// Empty means every Prüfungsteil.
    QStringList parts;
    /// Off by default: when a question is new anyway it is not worth asking
    /// whether it was due.
    bool includeFresh = true;
};

/// Plans and records a practice round.
///
/// In contrast to an examination this is not drawn at random. The order is the
/// one the scheduler produces, so the shaky questions come first and the ones
/// that are secure sink to the back. The learner reports how sure they felt,
/// because a correct answer that was guessed has to come back sooner than one
/// that was known.
class StudySession
{
public:
    StudySession(const Catalog &catalog, Scheduler *scheduler, StudyOptions options);

    /// Question ids in the order they should be practised.
    const QStringList &plan() const { return m_plan; }
    int count() const { return m_plan.size(); }

    /// The part a planned question belongs to, for the display.
    QString partLetterOf(const QString &questionId) const;

    /// Records the outcome together with the self reported confidence.
    ///
    /// The objective result is always recorded: a wrong answer is worth knowing
    /// even when the learner says nothing about how sure they were. Only the
    /// confidence is optional, and a round without a self report keeps the one
    /// that was given earlier instead of overwriting it with "unknown".
    void record(const QString &questionId, bool correct, Confidence confidence);

    /// Number of planned questions that the learner has never seen before.
    int freshCount() const;

private:
    const Catalog &m_catalog;
    Scheduler *m_scheduler;
    QStringList m_plan;
    QHash<QString, QString> m_partOfQuestion;
};

} // namespace hrt