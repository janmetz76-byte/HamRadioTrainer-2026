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
    if (!metadata.license.isEmpty() && metadata.attribution.isEmpty()) {
        add(issues, IssueSeverity::Error, QStringLiteral("metadata"),
            QStringLiteral("Es ist eine Lizenz ('%1') angegeben, aber kein Quellenvermerk "
                           "(attribution). Namensnennungs-Lizenzen wie DL-DE-BY-2.0 verlangen "
                           "den Quellenvermerk im Datensatz.").arg(metadata.license));
    }
    if (!metadata.attribution.isEmpty() && metadata.license.isEmpty()) {
        add(issues, IssueSeverity::Warning, QStringLiteral("metadata"),
            QStringLiteral("Es ist ein Quellenvermerk angegeben, aber keine Lizenz."));
    }
    if (!metadata.license.isEmpty() && !metadata.modifiedSinceSource) {
        add(issues, IssueSeverity::Warning, QStringLiteral("metadata"),
            QStringLiteral("Der Katalog stammt aus einem fremden Datensatz. Sobald er von Hand "
                           "geändert wird, muss modifiedSinceSource auf true gesetzt werden, "
                           "damit die Änderung im Quellenvermerk kenntlich ist."));
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
        int available = 0;
        for (const Section &section : part.sections) {
            available += section.activeQuestionCount();
            checkSection(section, basePath, metadata, issues, &seenIds);
        }

        const Selection &selection = part.selection;
        if (selection.questions > 0) {
            if (selection.questions > available) {
                add(issues, IssueSeverity::Error, part.id,
                    QStringLiteral("Prüfungsteil '%1' verlangt %2 Fragen, enthält aber nur %3 "
                                   "verfügbare (ungesperrte, nicht zurückgezogene) Fragen.")
                        .arg(part.id)
                        .arg(selection.questions)
                        .arg(available));
            }
            if (selection.errorPoints && selection.questions > 0
                && *selection.errorPoints >= selection.questions) {
                add(issues, IssueSeverity::Warning, part.id,
                    QStringLiteral("Prüfungsteil '%1': Fehlerpunkte (%2) sind nicht kleiner als "
                                   "die Frageanzahl (%3). Die Prüfung wäre dann immer zu "
                                   "bestehen.")
                        .arg(part.id)
                        .arg(*selection.errorPoints)
                        .arg(selection.questions));
            }
            if (selection.passScore && *selection.passScore > selection.questions) {
                add(issues, IssueSeverity::Error, part.id,
                    QStringLiteral("Prüfungsteil '%1': Bestehensschwelle (%2) liegt über der "
                                   "Frageanzahl (%3).")
                        .arg(part.id)
                        .arg(*selection.passScore)
                        .arg(selection.questions));
            }
            if (!metadata.defaultPoints
                && (selection.questions * metadata.defaultPoints) == 0) {
                add(issues, IssueSeverity::Warning, part.id,
                    QStringLiteral("Prüfungsteil '%1': Für die Punktewertung fehlt ein Wert. "
                                   "Ohne defaultPoints in den Metadaten lässt sich die "
                                   "Punktezahl nicht bestimmen.")
                        .arg(part.id));
            }
        }
    }

    if (metadata.defaultPoints <= 0) {
        int withoutPoints = 0;
        for (const Question *question : catalog.questions()) {
            if (question->effectivePoints(metadata) <= 0)
                ++withoutPoints;
        }
        if (withoutPoints > 0) {
            add(issues, IssueSeverity::Warning, QStringLiteral("metadata"),
                QStringLiteral("Für %1 von %2 Fragen sind keine Fehlerpunkte hinterlegt, weder "
                               "an der Frage noch über defaultPoints. Prüfungssimulationen "
                               "können das Fehlerpunktbudget der Prüfungsordnung dann nicht "
                               "einhalten.")
                    .arg(withoutPoints)
                    .arg(catalog.totalQuestionCount()));
        }
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
                                    const CatalogMetadata &metadata,
                                    QVector<CatalogIssue> *issues, QStringList *seenIds)
{
    if (section.chapters.isEmpty()) {
        add(issues, IssueSeverity::Error, section.id,
            QStringLiteral("Abschnitt '%1' enthält keine Kapitel.").arg(section.id));
    }

    for (const Chapter &chapter : section.chapters) {
        if (chapter.questions.isEmpty()) {
            add(issues, IssueSeverity::Warning, chapter.id,
                QStringLiteral("Kapitel '%1' enthält keine Fragen.").arg(chapter.id));
        }
        for (const Question &question : chapter.questions)
            checkQuestion(question, chapter.id, basePath, metadata, issues, seenIds);
    }
}

void CatalogValidator::checkQuestion(const Question &question, const QString &chapterId,
                                     const QString &basePath,
                                     const CatalogMetadata &metadata,
                                     QVector<CatalogIssue> *issues, QStringList *seenIds)
{
    const QString location = question.id;

    if (question.id.isEmpty()) {
        add(issues, IssueSeverity::Error, location, QStringLiteral("Frage ohne Kennung."));
    } else if (seenIds->contains(question.id)) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Die Kennung '%1' wird mehrfach verwendet.").arg(question.id));
    } else {
        seenIds->append(question.id);
    }
    Q_UNUSED(chapterId)

    if (question.text.isEmpty() && question.image.isEmpty()) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Frage '%1' hat weder Fragetext noch Fragebild.").arg(question.id));
    }

    if (question.answers.size() < 2) {
        add(issues, IssueSeverity::Error, location,
            QStringLiteral("Frage '%1' hat weniger als zwei Antwortmöglichkeiten.")
                .arg(question.id));
    }

    for (const Answer &answer : question.answers) {
        if (!answer.hasContent()) {
            add(issues, IssueSeverity::Error, location,
                QStringLiteral("Frage '%1': Antwort '%2' hat weder Text noch Bild.")
                    .arg(question.id, answer.id));
        }
        if (!answer.image.isEmpty()) {
            if (!isSafeRelativePath(answer.image)) {
                add(issues, IssueSeverity::Error, location,
                    QStringLiteral("Frage '%1': Bildpfad der Antwort '%2' muss relativ sein und "
                                   "darf das Katalogverzeichnis nicht verlassen.")
                        .arg(question.id, answer.id));
            } else if (!basePath.isEmpty() && !QFileInfo::exists(QDir(basePath).filePath(answer.image))) {
                add(issues, IssueSeverity::Warning, location,
                    QStringLiteral("Frage '%1': Antwortbild '%2' wurde nicht gefunden.")
                        .arg(question.id, answer.image));
            }
        }
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