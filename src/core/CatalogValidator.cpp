// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "CatalogValidator.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace hrt {
namespace {

void add(QVector<CatalogIssue> *issues, IssueSeverity severity, const QString &location,
         const QString &message)
{
    issues->append(CatalogIssue{severity, location, message});
}

/// Rejects absolute paths, backslashes and any attempt to escape the catalog
/// directory. The catalog must stay a self contained, portable folder.
bool isSafeRelativePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char('\\')))
        return false;
    if (path.size() > 1 && path.at(1) == QLatin1Char(':'))
        return false;
    const QStringList segments = path.split(QLatin1Char('/'));
    for (const QString &segment : segments) {
        if (segment == QLatin1String(".."))
            return false;
    }
    return true;
}

} // namespace

bool CatalogValidator::validate(const Catalog &catalog, QVector<CatalogIssue> *issues,
                                const QString &basePath)
{
    issues->clear();
    QStringList seenIds;

    if (catalog.schemaVersion != 1) {
        add(issues, IssueSeverity::Error, QStringLiteral("catalog"),
            QStringLiteral("Nicht unterstützte schemaVersion %1, erwartet 1.")
                .arg(catalog.schemaVersion));
    }

    const CatalogMetadata &metadata = catalog.metadata;
    if (metadata.title.isEmpty()) {
        add(issues, IssueSeverity::Error, QStringLiteral("metadata"),
            QStringLiteral("Der Titel des Katalogs fehlt."));
    }
    if (metadata.examId.isEmpty()) {
        add(issues, IssueSeverity::Warning, QStringLiteral("metadata"),
            QStringLiteral("Es ist keine Prüfungsnummer (examId) angegeben."));
    }
    if (!metadata.validFrom.isValid()) {
        add(issues, IssueSeverity::Error, QStringLiteral("metadata"),
            QStringLiteral("Es ist kein gültiges Datum 'validFrom' angegeben."));
    }
    if (metadata.validUntil.isValid() && metadata.validFrom.isValid()
        && metadata.validUntil < metadata.validFrom) {
        add(issues, IssueSeverity::Error, QStringLiteral("metadata"),
            QStringLiteral("'validUntil' liegt vor 'validFrom'."));
    }
    if (catalog.parts.isEmpty()) {
        add(issues, IssueSeverity::Error, QStringLiteral("catalog"),
            QStringLiteral("Der Katalog enthält keine Prüfungsteile."));
    }

    for (const Part &part : catalog.parts) {
        if (part.sections.isEmpty()) {
            add(issues, IssueSeverity::Error, part.id,
                QStringLiteral("Der Prüfungsteil '%1' enthält keine Abschnitte.").arg(part.id));
        }
        for (const Section &section : part.sections)
            checkSection(section, basePath, issues, &seenIds);
    }

    if (catalog.totalQuestionCount() == 0) {
        add(issues, IssueSeverity::Error, QStringLiteral("catalog"),
            QStringLiteral("Der Katalog enthält keine Fragen."));
    } else if (catalog.activeQuestionCount() == 0) {
        add(issues, IssueSeverity::Error, QStringLiteral("catalog"),
            QStringLiteral("Alle Fragen sind gesperrt oder zurückgezogen. Es kann keine "
                           "Prüfungssimulation erzeugt werden."));
    }

    for (const CatalogIssue &issue : *issues) {
        if (issue.severity == IssueSeverity::Error)
            return false;
    }
    return true;
}

void CatalogValidator::checkSection(const Section &section, const QString &basePath,
                                    QVector<CatalogIssue> *issues, QStringList *seenIds)
{
    if (section.chapters.isEmpty()) {
        add(issues, IssueSeverity::Error, section.id,
            QStringLiteral("Abschnitt '%1' enthält keine Kapitel.").arg(section.id));
        return;
    }

    if (section.selection.questions > 0) {
        const int available = section.activeQuestionCount();
        if (section.selection.questions > available) {
            add(issues, IssueSeverity::Error, section.id,
                QStringLiteral("Abschnitt '%1' verlangt %2 Fragen, enthält aber nur %3 "
                               "verfügbare (ungesperrte, nicht zurückgezogene) Fragen.")
                    .arg(section.id)
                    .arg(section.selection.questions)
                    .arg(available));
        }
        if (section.selection.errorPoints) {
            int budget = *section.selection.errorPoints;
            if (budget < section.selection.questions) {
                add(issues, IssueSeverity::Warning, section.id,
                    QStringLiteral("Abschnitt '%1': Fehlerpunktebudget (%2) ist kleiner als die "
                                   "geforderte Frageanzahl (%3).")
                        .arg(section.id)
                        .arg(budget)
                        .arg(section.selection.questions));
            }
        }
    }

    for (const Chapter &chapter : section.chapters) {
        if (chapter.questions.isEmpty()) {
            add(issues, IssueSeverity::Warning, chapter.id,
                QStringLiteral("Kapitel '%1' enthält keine Fragen.").arg(chapter.id));
        }
        for (const Question &question : chapter.questions)
            checkQuestion(question, chapter.id, basePath, issues, seenIds);
    }
}

void CatalogValidator::checkQuestion(const Question &question, const QString &chapterId,
                                     const QString &basePath, QVector<CatalogIssue> *issues,
                                     QStringList *seenIds)
{
    const QString location = question.id;

    if (question.id.isEmpty()) {
        add(issues, IssueSeverity::Error, location, QStringLiteral("Frage ohne Kennung."));
    } else if (seenIds->contains(question.id)) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Die Kennung '%1' wird mehrfach verwendet.").arg(question.id));
    } else {
        seenIds->append(question.id);
        if (!chapterId.isEmpty() && !question.id.startsWith(chapterId)) {
            add(issues, IssueSeverity::Warning, location,
                QStringLiteral("Kennung '%1' beginnt nicht mit der Kapitelkennung '%2'. Die "
                               "Zuordnung wird beim Nachtragen von Fragen unübersichtlich.")
                    .arg(question.id, chapterId));
        }
    }

    if (question.text.isEmpty()) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Frage '%1' hat keinen Fragetext.").arg(question.id));
    }

    if (question.answers.size() < 2) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Frage '%1' hat weniger als zwei Antwortmöglichkeiten.")
                .arg(question.id));
    }

    const QVector<int> correct = question.correctAnswerIndexes();
    if (correct.isEmpty()) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Frage '%1' hat keine als richtig markierte Antwort.").arg(question.id));
    } else if (question.type == QuestionType::Single && correct.size() > 1) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Frage '%1' ist als 'single' deklariert, hat aber %2 richtige Antworten.")
                .arg(question.id)
                .arg(correct.size()));
    } else if (question.type == QuestionType::Multiple && correct.size() < 2) {
        add(issues, IssueSeverity::Warning, location,
            QStringLiteral("Frage '%1' ist als 'multiple' deklariert, hat aber nur eine richtige "
                           "Antwort. Die Mehrfachauswahl ist damit nicht erforderlich.")
                .arg(question.id));
    }

    if (question.points <= 0) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Frage '%1' hat keine Fehlerpunkte (points).").arg(question.id));
    }

    if (!question.history.addedOn.isValid()) {
        add(issues, IssueSeverity::Warning, location,
            QStringLiteral("Frage '%1' hat kein Datum 'addedOn' in der Historie.")
                .arg(question.id));
    }
    if (question.isRetired() && question.history.changedOn.isValid()
        && question.history.changedOn < question.history.addedOn) {
        add(issues, IssueSeverity::Warning, location,
            QStringLiteral("Frage '%1': 'changedOn' liegt vor 'addedOn'.").arg(question.id));
    }
    if (question.isRetired() && !question.locked) {
        add(issues, IssueSeverity::Warning, location,
            QStringLiteral("Frage '%1' ist zurückgezogen, aber nicht gesperrt. Zurückgezogene "
                           "Fragen sollten zusätzlich gesperrt werden, damit sie in alten "
                           "Statistiken erhalten bleiben, aber nie neu gezogen werden.")
                .arg(question.id));
    }

    if (!question.image.isEmpty()) {
        if (!isSafeRelativePath(question.image)) {
            add(issues, IssueSeverity::Error, location,
                QStringLiteral("Frage '%1': Bildpfad '%2' muss relativ sein und darf das "
                               "Katalogverzeichnis nicht verlassen.")
                    .arg(question.id, question.image));
        } else if (!basePath.isEmpty()) {
            const QDir base(basePath);
            if (!QFileInfo::exists(base.filePath(question.image))) {
                add(issues, IssueSeverity::Warning, location,
                    QStringLiteral("Frage '%1': Bilddatei '%2' wurde nicht gefunden.")
                        .arg(question.id, question.image));
            }
        }
    }
}

} // namespace hrt