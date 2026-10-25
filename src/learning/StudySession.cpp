// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "StudySession.h"

#include <QDate>

namespace hrt {

StudySession::StudySession(const Catalog &catalog, Scheduler *scheduler, StudyOptions options)
    : m_catalog(catalog), m_scheduler(scheduler)
{
    const QDate today = QDate::currentDate();

    // Alle gewaehlten Teile in einen Topf. Teil fuer Teil abgearbeitet bestand
    // eine Runde eines neuen Katalogs nur aus Vorschriften.
    QStringList candidates;
    QStringList fresh;
    for (const Part &part : catalog.parts) {
        const QString letter = examPartLetter(part);
        if (!options.parts.isEmpty() && !options.parts.contains(letter))
            continue;

        for (const Section &section : part.sections) {
            for (const Question *question : section.questions()) {
                if (!question->isActive())
                    continue;

                m_partOfQuestion.insert(question->id, letter);

                const QuestionState state = scheduler->stateFor(question->id);
                if (options.onlyShaky && !(state.timesAnswered > 0 && state.box == 0))
                    continue;
                if (!options.includeFresh && state.isNew())
                    continue;
                if (!state.isNew()) {
                    if (options.onlyDue) {
                        if (state.box >= Scheduler::MaxBox)
                            continue;
                        if (state.dueOn.isValid() && state.dueOn > today)
                            continue;
                    }
                    candidates.append(question->id);
                    continue;
                }
                fresh.append(question->id);
            }
        }
    }

    // With "only due", the repetitions have to come before the questions that
    // were never seen. A fresh question is always offered, so a plain priority
    // order would fill a short round entirely with new questions and the due
    // ones would never be reached.
    if (options.onlyDue) {
        m_plan.append(scheduler->questionsByPriority(candidates, options.shuffleSeed));
        m_plan.append(scheduler->questionsByPriority(fresh, options.shuffleSeed));
    } else {
        // Shaky and new questions first, secure ones to the back.
        m_plan.append(scheduler->questionsByPriority(candidates + fresh, options.shuffleSeed));
    }

    if (options.maxQuestions > 0 && m_plan.size() > options.maxQuestions)
        m_plan = m_plan.mid(0, options.maxQuestions);
}

QString StudySession::partLetterOf(const QString &questionId) const
{
    return m_partOfQuestion.value(questionId);
}

void StudySession::record(const QString &questionId, bool correct, Confidence confidence)
{
    if (!m_scheduler)
        return;

    AnswerOutcome outcome;
    outcome.questionId = questionId;
    outcome.correct = correct;
    // Without a self report the previously reported confidence is kept, so a
    // later answer does not erase what the learner once said about the
    // question.
    outcome.confidence = confidence == Confidence::Unknown
                             ? m_scheduler->stateFor(questionId).confidence
                             : confidence;
    m_scheduler->record(outcome);
}

int StudySession::freshCount() const
{
    int count = 0;
    if (!m_scheduler)
        return count;
    for (const QString &id : m_plan) {
        if (m_scheduler->stateFor(id).isNew())
            ++count;
    }
    return count;
}

} // namespace hrt