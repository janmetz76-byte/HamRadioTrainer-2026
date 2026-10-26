// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "StudyPlan.h"

#include "learning/Scheduler.h"

namespace hrt {

StudyPlan computeStudyPlan(const LearningStatistics &statistics, const QDate &examDate,
                           const QDate &today)
{
    StudyPlan plan;
    plan.examDate = examDate;
    plan.questions = statistics.questions;
    plan.fresh = statistics.fresh;
    plan.mastered = statistics.mastered;
    plan.newToday = statistics.newToday;

    if (!examDate.isValid() || today.daysTo(examDate) < 0)
        return plan;

    plan.active = true;
    plan.daysLeft = int(today.daysTo(examDate));
    plan.finalPhase = plan.daysLeft < Scheduler::FinalPhaseDays;

    // Das Tagesziel bezieht sich auf den Stand vom Morgen, sonst sinkt es mit
    // jeder heute gelernten Frage. Bis zur Schlussphase verteilt, heute
    // mitgezaehlt; ist sie erreicht, bleibt nur noch heute.
    const int openThisMorning = statistics.fresh + statistics.newToday;
    const int daysForNew = plan.daysLeft - Scheduler::FinalPhaseDays + 1;
    if (daysForNew <= 1)
        plan.dailyNewTarget = openThisMorning;
    else
        plan.dailyNewTarget = (openThisMorning + daysForNew - 1) / daysForNew;
    return plan;
}

} // namespace hrt
