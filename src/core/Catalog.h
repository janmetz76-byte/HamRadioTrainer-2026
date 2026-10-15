// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ham Radio Trainer contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// In‑memory model of one question catalog for one examination class.
// Mirrors schema/catalog.xsd. See that file for the authoritative format
// definition and for the semantics of each field.
#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace hrt {

/// German amateur radio examination classes.
/// N = Novize, E = Einsteiger, A = Allgemein.
enum class ExamClass {
    Novize,
    Einsteiger,
    Allgemein,
};

QString examClassToString(ExamClass value);
std::optional<ExamClass> examClassFromString(const QString &value);

/// Whether one or several answers may be correct. The value is taken verbatim
/// from the file rather than derived, so that a hand edited question keeps its
/// declared type when the catalog is saved again.
enum class QuestionType {
    Single,    ///< genau eine richtige Antwort
    Multiple,  ///< Mehrfachauswahl, mindestens zwei richtige Antworten
};

QString questionTypeToString(QuestionType value);
std::optional<QuestionType> questionTypeFromString(const QString &value);

struct Answer
{
    QString id;
    QString text;
    bool correct = false;
    int order = 0;
};

/// Per question revision metadata. A retired question stays in the catalog so
/// that learning statistics of past sessions remain reproducible; the exam
/// simulator and the learner never present it again.
struct QuestionHistory
{
    QDate addedOn;
    QDate changedOn;
    QDate retiredOn;

    bool isRetired() const { return retiredOn.isValid(); }
};

struct Question
{
    QString id;
    int order = 0;
    QString text;
    QuestionType type = QuestionType::Single;
    QVector<Answer> answers;
    QString image;        ///< Path relative to the catalog file, empty if none.
    QString explanation;
    QuestionHistory history;
    int points = 0;       ///< Fehlerpunkte charged per wrong answer.
    bool locked = false;  ///< Gesperrt: excluded from generated papers.

    bool isRetired() const { return history.isRetired(); }
    /// Available in generated papers and in the learner.
    bool isActive() const { return !locked && !history.isRetired(); }
    /// True when more than one answer is correct (Mehrfachauswahl).
    bool isMultipleChoice() const { return type == QuestionType::Multiple; }
    QVector<int> correctAnswerIndexes() const;
};

struct Chapter
{
    QString id;
    QString title;
    int order = 0;
    QVector<Question> questions;
};

/// How the exam simulator draws a paper for a section.
/// "questions" is mandatory, "errorPoints" is an optional target budget that
/// constrains which question types may be drawn.
struct Selection
{
    int questions = 0;
    std::optional<int> errorPoints;
};

struct Section
{
    QString id;
    QString title;
    int order = 0;
    Selection selection;
    QVector<Chapter> chapters;

    QVector<Question *> questions();
    QVector<const Question *> questions() const;
    int activeQuestionCount() const;
};

struct Part
{
    QString id;
    QString title;
    int order = 0;
    QVector<Section> sections;
};

struct CatalogMetadata
{
    QString title;
    ExamClass examClass = ExamClass::Einsteiger;
    QString examId;
    QString authority;
    QString catalogVersion;
    QDate validFrom;
    QDate validUntil;
    QString language = QStringLiteral("de");
    QString sourceUrl;
    QString notes;
};

struct Catalog
{
    int schemaVersion = 1;
    CatalogMetadata metadata;
    QVector<Part> parts;

    QVector<Question *> questions();
    QVector<const Question *> questions() const;
    int totalQuestionCount() const { return questions().size(); }
    int activeQuestionCount() const;
};

} // namespace hrt