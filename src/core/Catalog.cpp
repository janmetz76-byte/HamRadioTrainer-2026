// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "Catalog.h"

#include <algorithm>

namespace hrt {

QString examClassToString(ExamClass value)
{
    switch (value) {
    case ExamClass::Novize:
        return QStringLiteral("N");
    case ExamClass::Einsteiger:
        return QStringLiteral("E");
    case ExamClass::Allgemein:
        return QStringLiteral("A");
    }
    return QStringLiteral("E");
}

std::optional<ExamClass> examClassFromString(const QString &value)
{
    const QString normalized = value.trimmed().toUpper();
    if (normalized == QLatin1String("N"))
        return ExamClass::Novize;
    if (normalized == QLatin1String("E"))
        return ExamClass::Einsteiger;
    if (normalized == QLatin1String("A"))
        return ExamClass::Allgemein;
    return std::nullopt;
}

QString textFormatToString(TextFormat value)
{
    return value == TextFormat::Latex ? QStringLiteral("latex") : QStringLiteral("plain");
}

std::optional<TextFormat> textFormatFromString(const QString &value)
{
    if (value == QLatin1String("plain"))
        return TextFormat::Plain;
    if (value == QLatin1String("latex"))
        return TextFormat::Latex;
    return std::nullopt;
}

int Question::effectivePoints(const CatalogMetadata &metadata) const
{
    return points > 0 ? points : metadata.defaultPoints;
}

QString questionTypeToString(QuestionType value)
{
    return value == QuestionType::Multiple ? QStringLiteral("multiple")
                                           : QStringLiteral("single");
}

std::optional<QuestionType> questionTypeFromString(const QString &value)
{
    if (value == QLatin1String("single"))
        return QuestionType::Single;
    if (value == QLatin1String("multiple"))
        return QuestionType::Multiple;
    return std::nullopt;
}

QVector<int> Question::correctAnswerIndexes() const
{
    QVector<int> result;
    for (int i = 0; i < answers.size(); ++i) {
        if (answers.at(i).correct)
            result.append(i);
    }
    return result;
}

QVector<Question *> Section::questions()
{
    QVector<Question *> result;
    for (Chapter &chapter : chapters) {
        for (Question &question : chapter.questions)
            result.append(&question);
    }
    return result;
}

QVector<const Question *> Section::questions() const
{
    QVector<const Question *> result;
    for (const Chapter &chapter : chapters) {
        for (const Question &question : chapter.questions)
            result.append(&question);
    }
    return result;
}

int Section::activeQuestionCount() const
{
    int count = 0;
    for (const Question *question : questions()) {
        if (question->isActive())
            ++count;
    }
    return count;
}

QVector<Question *> Catalog::questions()
{
    QVector<Question *> result;
    for (Part &part : parts) {
        for (Section &section : part.sections) {
            const QVector<Question *> sectionQuestions = section.questions();
            result.append(sectionQuestions);
        }
    }
    return result;
}

QVector<const Question *> Catalog::questions() const
{
    QVector<const Question *> result;
    for (const Part &part : parts) {
        for (const Section &section : part.sections)
            result.append(section.questions());
    }
    return result;
}

int Catalog::activeQuestionCount() const
{
    int count = 0;
    for (const Part &part : parts) {
        for (const Section &section : part.sections)
            count += section.activeQuestionCount();
    }
    return count;
}

} // namespace hrt