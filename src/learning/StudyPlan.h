// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "learning/LearningStatistics.h"

#include <QDate>

namespace hrt {

/// What the exam date asks of today.
///
/// The never seen questions are spread evenly over the days up to the final
/// phase, so that every question has been seen once before the last week and
/// that week is left for repeating. The quota is computed again every day from
/// what is still open: a learner who falls behind gets a higher quota, one who
/// is ahead a lower one.
struct StudyPlan
{
    /// False without an exam date or once it has passed. All other figures
    /// are then meaningless.
    bool active = false;
    QDate examDate;
    int daysLeft = -1;          ///< 0 on the exam day
    bool finalPhase = false;    ///< the last Scheduler::FinalPhaseDays days

    int questions = 0;
    int fresh = 0;              ///< never seen
    int mastered = 0;
    int newToday = 0;           ///< seen for the first time today
    int dailyNewTarget = 0;     ///< new questions to take on today, newToday included

    /// New questions still to do today.
    int newRemainingToday() const { return dailyNewTarget > newToday ? dailyNewTarget - newToday : 0; }

    /// Share of the questions in a secure Leitner box, in percent. It is the
    /// figure that tells how far the preparation has come, not a forecast of
    /// the exam result; the exam simulation answers that.
    double readiness() const
    {
        return questions > 0 ? 100.0 * double(mastered) / double(questions) : 0.0;
    }
};

/// The plan for \a today, derived from the statistics of the chosen parts.
StudyPlan computeStudyPlan(const LearningStatistics &statistics, const QDate &examDate,
                           const QDate &today = QDate::currentDate());

} // namespace hrt
