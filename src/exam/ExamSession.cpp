// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ExamSession.h"

#include <algorithm>

namespace hrt {

QStringList ExamResult::failedParts() const
{
    QStringList failed;
    for (const PartResult &part : parts) {
        if (!part.passed())
            failed.append(part.letter);
    }
    return failed;
}

QString ExamResult::oralRetestPart() const
{
    const PartResult *failed = nullptr;
    for (const PartResult &part : parts) {
        if (part.passed())
            continue;
        if (failed)
            return {};
        failed = &part;
    }
    if (!failed || failed->achievedPoints < kOralRetestMinimum)
        return {};
    return failed->letter;
}

int ExamResult::weakestShortfall() const
{
    int shortfall = 0;
    bool seen = false;
    for (const PartResult &part : parts) {
        const int missing = part.passScore - part.achievedPoints;
        if (!seen || missing > shortfall) {
            shortfall = missing;
            seen = true;
        }
    }
    return shortfall;
}

ExamSession::ExamSession(const Catalog &catalog, const ExamPaper &paper)
    : m_catalog(catalog), m_paper(paper)
{
    m_byId = questionIndex(catalog);

    m_selected.assign(m_paper.questions.size(), -1);

    for (const PaperPartSummary &summary : m_paper.parts) {
        PartResult result;
        result.partId = summary.partId;
        result.letter = summary.letter;
        result.title = summary.title;
        result.passScore = summary.passScore;
        result.errorPointBudget = summary.errorPointBudget;
        m_partMeta.append(result);
    }
}

const Question *ExamSession::questionAt(int index) const
{
    if (index < 0 || index >= m_paper.questions.size())
        return nullptr;
    return m_byId.value(m_paper.questions.at(index).questionId, nullptr);
}

const QVector<int> &ExamSession::answerOrderAt(int index) const
{
    static const QVector<int> empty;
    if (index < 0 || index >= m_paper.questions.size())
        return empty;
    return m_paper.questions.at(index).answerOrder;
}

QString ExamSession::partLetterAt(int index) const
{
    if (index < 0 || index >= m_paper.questions.size())
        return QString();
    return m_paper.questions.at(index).partLetter;
}

QString ExamSession::partTitleAt(int index) const
{
    if (index < 0 || index >= m_paper.questions.size())
        return QString();
    const QString partId = m_paper.questions.at(index).partId;
    for (const PartResult &part : m_partMeta) {
        if (part.partId == partId)
            return part.title;
    }
    return QString();
}

void ExamSession::answer(int index, int displayedPosition)
{
    if (index < 0 || index >= m_selected.size())
        return;
    if (displayedPosition < 0 || displayedPosition >= answerOrderAt(index).size())
        return;
    m_selected[index] = displayedPosition;
}

int ExamSession::selectedPosition(int index) const
{
    if (index < 0 || index >= m_selected.size())
        return -1;
    return m_selected.at(index);
}

bool ExamSession::isAnswered(int index) const
{
    return selectedPosition(index) >= 0;
}

int ExamSession::answeredCount() const
{
    return static_cast<int>(std::count_if(m_selected.cbegin(), m_selected.cend(),
                                          [](int value) { return value >= 0; }));
}

int ExamSession::countOfPart(const QString &letter) const
{
    return static_cast<int>(std::count_if(
        m_paper.questions.cbegin(), m_paper.questions.cend(),
        [&letter](const PaperQuestion &entry) { return entry.partLetter == letter; }));
}

int ExamSession::answeredCountOfPart(const QString &letter) const
{
    int count = 0;
    for (int i = 0; i < m_paper.questions.size(); ++i) {
        if (partLetterAt(i) == letter && isAnswered(i))
            ++count;
    }
    return count;
}

int ExamSession::nextUnanswered(int from) const
{
    for (int i = std::max(0, from); i < m_selected.size(); ++i) {
        if (m_selected.at(i) < 0)
            return i;
    }
    return -1;
}

int ExamSession::correctPositionAt(int index) const
{
    const Question *question = questionAt(index);
    const QVector<int> &order = answerOrderAt(index);
    if (!question || order.isEmpty())
        return -1;
    for (int position = 0; position < order.size(); ++position) {
        const int answerIndex = order.at(position);
        if (answerIndex >= 0 && answerIndex < question->answers.size()
            && question->answers.at(answerIndex).correct)
            return position;
    }
    return -1;
}

ExamResult ExamSession::evaluate() const
{
    ExamResult result;
    result.parts = m_partMeta;
    result.maxPoints = m_paper.totalPoints;
    result.passScore = m_paper.passScore;
    result.errorPointBudget = m_paper.errorPointBudget;
    result.totalQuestions = count();

    QHash<QString, int> partOfLetter;
    for (int i = 0; i < result.parts.size(); ++i)
        partOfLetter.insert(result.parts.at(i).letter, i);

    bool everyPartPassed = !result.parts.isEmpty();

    for (int i = 0; i < m_paper.questions.size(); ++i) {
        const Question *question = questionAt(i);
        if (!question)
            continue;

        const int points = question->effectivePoints(m_catalog.metadata);
        const bool answered = isAnswered(i);
        const bool correct =
            answered && selectedPosition(i) == correctPositionAt(i) && correctPositionAt(i) >= 0;

        if (correct) {
            result.achievedPoints += points;
        } else {
            ++result.wrongAnswers;
            if (!answered)
                ++result.unanswered;
        }

        const int partIndex = partOfLetter.value(partLetterAt(i), -1);
        if (partIndex < 0)
            continue;
        PartResult &part = result.parts[partIndex];
        ++part.questionCount;
        part.maxPoints += points;
        if (correct) {
            part.achievedPoints += points;
        } else {
            ++part.wrongAnswers;
            if (!answered)
                ++part.unanswered;
            part.failedQuestionIds.append(question->id);
        }
    }

    for (const PartResult &part : result.parts) {
        if (!part.passed())
            everyPartPassed = false;
    }
    result.passed = everyPartPassed;

    return result;
}

} // namespace hrt