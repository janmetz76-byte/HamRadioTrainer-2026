// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "LearningStatistics.h"

namespace hrt {
namespace {

/// A question in the second to last box still comes back after 16 days; in the
/// last box only after 35. Everything below that is still worth practising.
constexpr int kMasteredBox = Scheduler::MaxBox - 1;

/// True when the Leitner spacing offers the question today. Questions that were
/// never answered are always due, and so are the ones in the top box only as a
/// long-term guard against forgetting.
bool isDue(const QuestionState &state, const QDate &today)
{
    if (state.box >= Scheduler::MaxBox)
        return false;
    if (!state.dueOn.isValid())
        return true;
    return state.dueOn <= today;
}

} // namespace

QSet<int> effectiveParts(const Catalog &catalog, const QSet<int> &selected)
{
    QSet<int> parts;
    for (int part : selected) {
        if (part >= 0 && part < catalog.parts.size())
            parts.insert(part);
    }
    if (parts.isEmpty()) {
        for (int i = 0; i < catalog.parts.size(); ++i)
            parts.insert(i);
    }
    return parts;
}

LearningStatistics computeStatistics(const Catalog &catalog, const Scheduler &scheduler,
                                     const QDate &today, const QSet<int> &parts)
{
    LearningStatistics statistics;
    statistics.byBox.assign(Scheduler::MaxBox + 1, 0);
    const QSet<int> counted = effectiveParts(catalog, parts);

    QHash<QString, int> indexOfLetter;
    for (int i = 0; i < catalog.parts.size(); ++i) {
        if (!counted.contains(i))
            continue;
        PartProgress progress;
        progress.partId = catalog.parts.at(i).id;
        progress.letter = examPartLetter(catalog.parts.at(i));
        progress.title = catalog.parts.at(i).title;
        indexOfLetter.insert(progress.letter, statistics.parts.size());
        statistics.parts.append(progress);
    }

    // Questions are visited through their Prüfungsteil, so each one is counted
    // in the part it is actually asked in.
    for (int i = 0; i < catalog.parts.size(); ++i) {
        if (!counted.contains(i))
            continue;
        const Part &part = catalog.parts.at(i);
        const int partIndex = indexOfLetter.value(examPartLetter(part), -1);
        PartProgress *partProgress = partIndex >= 0 ? &statistics.parts[partIndex] : nullptr;

        for (const Section &section : part.sections) {
            for (const Question *question : section.questions()) {
                if (!question->isActive())
                    continue;

                ++statistics.questions;
                if (partProgress)
                    ++partProgress->questions;

                const QuestionState state = scheduler.stateFor(question->id);
                if (state.isNew()) {
                    ++statistics.fresh;
                    continue;
                }

                ++statistics.answered;
                statistics.timesAnswered += state.timesAnswered;
                statistics.timesCorrect += state.timesCorrect;
                ++statistics.byBox[qBound(0, state.box, Scheduler::MaxBox)];

                if (partProgress) {
                    ++partProgress->answered;
                    partProgress->timesAnswered += state.timesAnswered;
                    partProgress->timesCorrect += state.timesCorrect;
                }

                if (state.box == 0) {
                    ++statistics.shaky;
                    if (partProgress)
                        ++partProgress->shaky;
                } else if (state.box >= kMasteredBox) {
                    ++statistics.mastered;
                    if (partProgress)
                        ++partProgress->mastered;
                }

                if (isDue(state, today)) {
                    ++statistics.due;
                    if (partProgress)
                        ++partProgress->due;
                }
            }
        }
    }

    return statistics;
}

} // namespace hrt