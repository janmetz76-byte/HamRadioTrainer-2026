// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "Scheduler.h"

#include <algorithm>

namespace hrt {

QString confidenceToString(Confidence value)
{
    switch (value) {
    case Confidence::Guessing:
        return QStringLiteral("geraten");
    case Confidence::Unsure:
        return QStringLiteral("unsicher");
    case Confidence::FairlySure:
        return QStringLiteral("ziemlich sicher");
    case Confidence::Certain:
        return QStringLiteral("sicher");
    case Confidence::Unknown:
        break;
    }
    return QStringLiteral("unbekannt");
}

Scheduler::Scheduler(QVector<QuestionState> states)
    : m_states(std::move(states))
{
}

QuestionState Scheduler::stateFor(const QString &questionId) const
{
    for (const QuestionState &state : m_states) {
        if (state.questionId == questionId)
            return state;
    }
    QuestionState fresh;
    fresh.questionId = questionId;
    return fresh;
}

QuestionState *Scheduler::findOrCreate(const QString &questionId)
{
    for (QuestionState &state : m_states) {
        if (state.questionId == questionId)
            return &state;
    }
    QuestionState fresh;
    fresh.questionId = questionId;
    m_states.append(fresh);
    return &m_states.last();
}

const QuestionState &Scheduler::record(const AnswerOutcome &outcome)
{
    QuestionState *state = findOrCreate(outcome.questionId);
    const QDate today = QDate::currentDate();

    ++state->timesAnswered;
    state->confidence = outcome.confidence;
    state->lastSeen = today;

    if (outcome.correct) {
        ++state->timesCorrect;
        ++state->consecutiveCorrect;

        // A correct answer the learner only guessed moves the question less far
        // than one that felt certain.
        const int gain = (outcome.confidence == Confidence::Certain)      ? 2
                         : (outcome.confidence == Confidence::FairlySure) ? 1
                                                                          : 0;
        state->box = std::min(MaxBox, state->box + 1 + gain);
    } else {
        state->consecutiveCorrect = 0;
        state->box = std::max(0, state->box - 2);
    }

    // Spacing: the further along in the boxes, the longer the wait. A box that
    // was lowered comes back quickly.
    static const int daysPerBox[] = {0, 1, 3, 7, 16, 35};
    const int days = daysPerBox[std::clamp(state->box, 0, MaxBox)];
    state->dueOn = today.addDays(days);

    return *state;
}

double Scheduler::weightFor(const QuestionState &state) const
{
    // Never answered: high, so that every question is seen once before any
    // question is repeated.
    if (state.isNew())
        return 10.0;

    double weight = 1.0;

    // Leitner box: each box halves the likelihood again.
    weight *= std::pow(0.45, state.box);

    // Self reported certainty suppresses the question further. This is the
    // behaviour of the original trainer: a question the learner answered
    // securely comes back markedly less often than one they were unsure about.
    switch (state.confidence) {
    case Confidence::Certain:
        weight *= 0.30;
        break;
    case Confidence::FairlySure:
        weight *= 0.55;
        break;
    case Confidence::Unsure:
        weight *= 1.60;
        break;
    case Confidence::Guessing:
        weight *= 2.20;
        break;
    case Confidence::Unknown:
        weight *= 1.20;
        break;
    }

    // Recency: do not ask the same question twice within one session.
    if (state.lastSeen == QDate::currentDate())
        weight *= 0.05;

    // Staleness: the longer it has been, the more attractive again.
    if (state.lastSeen.isValid()) {
        const int daysSince = state.lastSeen.daysTo(QDate::currentDate());
        weight *= 1.0 + std::min(daysSince, 60) / 60.0;
    }

    // A question that is not due yet is pushed back.
    if (state.dueOn.isValid() && state.dueOn > QDate::currentDate())
        weight *= 0.25;

    return weight;
}

QVector<QString> Scheduler::questionsByPriority(const QStringList &candidateIds) const
{
    QVector<QPair<double, QString>> scored;
    scored.reserve(candidateIds.size());
    for (const QString &id : candidateIds)
        scored.append({weightFor(stateFor(id)), id});

    std::sort(scored.begin(), scored.end(), [](const auto &lhs, const auto &rhs) {
        if (lhs.first != rhs.first)
            return lhs.first > rhs.first;
        return lhs.second < rhs.second; // stable, reproducible
    });

    QVector<QString> ordered;
    ordered.reserve(scored.size());
    for (const auto &entry : scored)
        ordered.append(entry.second);
    return ordered;
}

QVector<int> Scheduler::distribution() const
{
    QVector<int> counts(MaxBox + 1, 0);
    for (const QuestionState &state : m_states) {
        const int box = std::clamp(state.box, 0, MaxBox);
        ++counts[box];
    }
    return counts;
}

void Scheduler::reset()
{
    m_states.clear();
}

} // namespace hrt