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
#include <QHash>
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

/// How the math markup inside a question is encoded.
enum class TextFormat {
    Plain,  ///< no markup
    Latex,  ///< $...$ inline markup, rendered by LatexFormatter
};

QString textFormatToString(TextFormat value);
std::optional<TextFormat> textFormatFromString(const QString &value);

struct Answer
{
    QString id;
    /// Empty when the answer consists only of a picture.
    QString text;
    /// Path relative to the catalog file, empty when the answer is text only.
    QString image;
    bool correct = false;
    int order = 0;

    bool hasContent() const { return !text.isEmpty() || !image.isEmpty(); }
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

struct CatalogMetadata;

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
    int points = 0;       ///< Fehlerpunkte charged per wrong answer, 0 = unknown.
    TextFormat textFormat = TextFormat::Plain;
    bool locked = false;  ///< Gesperrt: excluded from generated papers.

    bool isRetired() const { return history.isRetired(); }
    /// Available in generated papers and in the learner.
    bool isActive() const { return !locked && !history.isRetired(); }
    /// Penalty per wrong answer, falling back to the catalog default.
    int effectivePoints(const CatalogMetadata &metadata) const;
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

/// Prüfungsumfang of one amtlicher Prüfungsteil, fixed by
/// Amtsblatt-Verfügung 29/2024: 25 questions, 19 of which must be correct,
/// so six wrong answers are tolerated. The duration limit is informational.
struct Selection
{
    int questions = 0;
    std::optional<int> errorPoints;  ///< wrong answers still tolerated
    std::optional<int> passScore;    ///< points required to pass
    std::optional<int> minutes;      ///< time limit
};

struct Section
{
    QString id;
    QString title;
    int order = 0;
    QVector<Chapter> chapters;

    QVector<Question *> questions();
    QVector<const Question *> questions() const;
    int activeQuestionCount() const;
};

/// One amtlicher Prüfungsteil (V, B, N, E or A).
struct Part
{
    QString id;
    QString title;
    int order = 0;
    Selection selection;
    QVector<Section> sections;
};

/// Letter of a Prüfungsteil, taken from the last segment of its id
/// ("DE-E-teil4-e" -> "E"). Empty when the id does not end in a single letter.
///
/// The catalogs name their parts this way, so this is the one place that knows
/// how a letter is read back from a catalog.
QString examPartLetter(const Part &part);

struct CatalogMetadata
{
    QString title;
    /// Penalty per wrong answer used for questions without "points".
    int defaultPoints = 0;
    /// SPDX style licence identifier of the question text, e.g. DL-DE-BY-2.0.
    QString license;
    /// The same licence in words, as the publisher names it, e.g.
    /// "Datenlizenz Deutschland – Namensnennung – Version 2.0". The identifier
    /// alone tells a reader nothing about what the licence permits.
    QString licenseName;
    /// Mandatory source notice, verbatim, whenever "license" is set. Names the
    /// work and where it came from; the licence in words belongs in
    /// "licenseName".
    QString attribution;
    /// Set as soon as the catalog was edited after the import.
    bool modifiedSinceSource = false;
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
    /// SHA-256 over the catalog document, filled in when it is read and never
    /// written back. Learning progress is bound to this digest, so hand edited
    /// catalogs do not silently keep the progress of an earlier revision.
    QString sourceDigest;

    int schemaVersion = 1;
    CatalogMetadata metadata;
    QVector<Part> parts;

    QVector<Question *> questions();
    QVector<const Question *> questions() const;
    int totalQuestionCount() const { return questions().size(); }
    int activeQuestionCount() const;
};

/// Index of every question of a catalog by its id.
///
/// The catalog is a tree of parts, sections and chapters, so looking a single
/// question up means walking it. Views that work on single questions instead of
/// walking the tree need this map, and building it once keeps them from
/// repeating the same loop.
QHash<QString, const Question *> questionIndex(const Catalog &catalog);


} // namespace hrt