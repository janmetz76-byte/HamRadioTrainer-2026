// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "CatalogXml.h"

#include <QFile>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace hrt {
namespace {

QString readerError(const QXmlStreamReader &reader, const QString &contextName)
{
    const QString where =
        contextName.isEmpty() ? QStringLiteral("Zeile %1").arg(reader.lineNumber())
                              : QStringLiteral("%1, Zeile %2").arg(contextName).arg(reader.lineNumber());
    return QStringLiteral("XML-Fehler in %1: %2").arg(where, reader.errorString());
}

QDate readDate(QXmlStreamReader &reader)
{
    return QDate::fromString(reader.readElementText().trimmed(), Qt::ISODate);
}

int readInt(QXmlStreamReader &reader, bool *ok)
{
    *ok = true;
    bool parsed = false;
    const int value = reader.readElementText().trimmed().toInt(&parsed);
    *ok = parsed;
    return value;
}

/// Reads the fixed child sequence of the <metadata> element.
bool readMetadata(QXmlStreamReader &reader, CatalogMetadata *metadata, QString *error)
{
    while (reader.readNextStartElement()) {
        const QString name = reader.name().toString();
        if (name == QLatin1String("title")) {
            metadata->title = reader.readElementText().trimmed();
        } else if (name == QLatin1String("examClass")) {
            const auto parsed = examClassFromString(reader.readElementText());
            if (!parsed) {
                *error = QStringLiteral("Unbekannte Prüfungsklasse '%1'.").arg(reader.readElementText());
                return false;
            }
            metadata->examClass = *parsed;
        } else if (name == QLatin1String("examId")) {
            metadata->examId = reader.readElementText().trimmed();
        } else if (name == QLatin1String("authority")) {
            metadata->authority = reader.readElementText().trimmed();
        } else if (name == QLatin1String("catalogVersion")) {
            metadata->catalogVersion = reader.readElementText().trimmed();
        } else if (name == QLatin1String("validFrom")) {
            metadata->validFrom = readDate(reader);
        } else if (name == QLatin1String("validUntil")) {
            metadata->validUntil = readDate(reader);
        } else if (name == QLatin1String("language")) {
            metadata->language = reader.readElementText().trimmed();
        } else if (name == QLatin1String("defaultPoints")) {
            bool pointsOk = false;
            metadata->defaultPoints = reader.readElementText().trimmed().toInt(&pointsOk);
            if (!pointsOk) {
                *error = QStringLiteral("'defaultPoints' ist keine Zahl.");
                return false;
            }
        } else if (name == QLatin1String("license")) {
            metadata->license = reader.readElementText().trimmed();
        } else if (name == QLatin1String("attribution")) {
            metadata->attribution = reader.readElementText().trimmed();
        } else if (name == QLatin1String("modifiedSinceSource")) {
            metadata->modifiedSinceSource =
                reader.readElementText().trimmed() == QLatin1String("true");
        } else if (name == QLatin1String("sourceUrl")) {
            metadata->sourceUrl = reader.readElementText().trimmed();
        } else if (name == QLatin1String("notes")) {
            metadata->notes = reader.readElementText().trimmed();
        } else {
            *error = QStringLiteral("Unbekanntes Metadaten-Element '%1'.").arg(name);
            return false;
        }
    }
    return true;
}

bool readQuestion(QXmlStreamReader &reader, Question *question, QString *error)
{
    question->id = reader.attributes().value(QLatin1String("id")).toString();
    bool ok = false;
    question->order = reader.attributes().value(QLatin1String("order")).toInt(&ok);
    if (!ok) {
        *error = QStringLiteral("Frage ohne gültiges Attribut 'order'.");
        return false;
    }
    if (reader.attributes().hasAttribute(QLatin1String("points"))) {
        question->points = reader.attributes().value(QLatin1String("points")).toInt(&ok);
        if (!ok) {
            *error = QStringLiteral("Frage %1: Attribut 'points' ist keine Zahl.").arg(question->id);
            return false;
        }
    }
    // textFormat defaults to "plain" in the schema, so an absent attribute is
    // not an error.
    const QString formatValue = reader.attributes().value(QLatin1String("textFormat")).toString();
    if (formatValue.isEmpty()) {
        question->textFormat = TextFormat::Plain;
    } else {
        const auto format = textFormatFromString(formatValue);
        if (!format) {
            *error = QStringLiteral("Frage %1: Attribut 'textFormat' muss 'plain' oder "
                                   "'latex' sein.").arg(question->id);
            return false;
        }
        question->textFormat = *format;
    }
    question->locked =
        reader.attributes().value(QLatin1String("locked")).toString() == QLatin1String("true");
    const auto type =
        questionTypeFromString(reader.attributes().value(QLatin1String("type")).toString());
    if (!type) {
        *error = QStringLiteral("Frage %1: Attribut 'type' muss 'single' oder 'multiple' sein.")
                     .arg(question->id);
        return false;
    }
    question->type = *type;

    while (reader.readNextStartElement()) {
        const QString name = reader.name().toString();
        if (name == QLatin1String("text")) {
            question->text = reader.readElementText().trimmed();
        } else if (name == QLatin1String("answers")) {
            while (reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("answer")) {
                    *error = QStringLiteral("Unerwartetes Element '%1' unter <answers>.")
                                 .arg(reader.name().toString());
                    return false;
                }
                Answer answer;
                answer.id = reader.attributes().value(QLatin1String("id")).toString();
                answer.text = reader.attributes().value(QLatin1String("text")).toString();
                answer.image = reader.attributes().value(QLatin1String("image")).toString();
                answer.order = reader.attributes().value(QLatin1String("order")).toInt(&ok);
                if (!ok) {
                    *error = QStringLiteral("Antwort ohne gültiges Attribut 'order'.");
                    return false;
                }
                const QString correct =
                    reader.attributes().value(QLatin1String("correct")).toString();
                if (correct != QLatin1String("true") && correct != QLatin1String("false")) {
                    *error = QStringLiteral("Antwort %1: Attribut 'correct' muss 'true' oder "
                                           "'false' sein.")
                                 .arg(answer.id);
                    return false;
                }
                answer.correct = (correct == QLatin1String("true"));
                question->answers.append(answer);
                // <answer/> is empty. readNextStartElement() does not step over
                // the EndElement of the element it is already positioned on, so
                // that single readNext() is required here.
                reader.readNext();
            }
        } else if (name == QLatin1String("image")) {
            question->image = reader.readElementText().trimmed();
        } else if (name == QLatin1String("explanation")) {
            question->explanation = reader.readElementText().trimmed();
        } else if (name == QLatin1String("history")) {
            while (reader.readNextStartElement()) {
                const QString historyName = reader.name().toString();
                if (historyName == QLatin1String("addedOn"))
                    question->history.addedOn = readDate(reader);
                else if (historyName == QLatin1String("changedOn"))
                    question->history.changedOn = readDate(reader);
                else if (historyName == QLatin1String("retiredOn"))
                    question->history.retiredOn = readDate(reader);
                else {
                    *error = QStringLiteral("Unbekanntes Historien-Element '%1'.").arg(historyName);
                    return false;
                }
            }
        } else {
            *error = QStringLiteral("Unbekanntes Frage-Element '%1'.").arg(name);
            return false;
        }
    }
    return true;
}

} // namespace

bool CatalogXml::loadFromString(const QByteArray &document, Catalog *catalog, QString *error,
                                const QString &contextName)
{
    QXmlStreamReader reader(document);

    if (!reader.readNextStartElement() || reader.name() != QLatin1String("catalog")) {
        *error = contextName.isEmpty()
                     ? QStringLiteral("Wurzelelement ist nicht <catalog>.")
                     : QStringLiteral("%1: Wurzelelement ist nicht <catalog>.").arg(contextName);
        return false;
    }

    bool schemaOk = false;
    catalog->schemaVersion =
        reader.attributes().value(QLatin1String("schemaVersion")).toInt(&schemaOk);
    if (!schemaOk) {
        *error = QStringLiteral("Attribut 'schemaVersion' fehlt oder ist keine Zahl.");
        return false;
    }

    while (reader.readNextStartElement()) {
        const QString name = reader.name().toString();
        if (name == QLatin1String("metadata")) {
            if (!readMetadata(reader, &catalog->metadata, error)) {
                if (error->isEmpty())
                    *error = readerError(reader, contextName);
                return false;
            }
        } else if (name == QLatin1String("parts")) {
            while (reader.readNextStartElement()) {
                if (reader.name() != QLatin1String("part")) {
                    *error = QStringLiteral("Unerwartetes Element '%1' unter <parts>.")
                                 .arg(reader.name().toString());
                    return false;
                }
                Part part;
                part.id = reader.attributes().value(QLatin1String("id")).toString();
                part.title = reader.attributes().value(QLatin1String("title")).toString();
                part.order = reader.attributes().value(QLatin1String("order")).toInt();
                while (reader.readNextStartElement()) {
                    if (reader.name() == QLatin1String("selection")) {
                        part.selection.questions =
                            reader.attributes().value(QLatin1String("questions")).toInt();
                        const QXmlStreamAttributes attributes = reader.attributes();
                        if (attributes.hasAttribute(QLatin1String("errorPoints")))
                            part.selection.errorPoints =
                                attributes.value(QLatin1String("errorPoints")).toInt();
                        if (attributes.hasAttribute(QLatin1String("passScore")))
                            part.selection.passScore =
                                attributes.value(QLatin1String("passScore")).toInt();
                        if (attributes.hasAttribute(QLatin1String("minutes")))
                            part.selection.minutes =
                                attributes.value(QLatin1String("minutes")).toInt();
                        reader.readNext(); // <selection/> is empty
                        continue;
                    }
                    if (reader.name() != QLatin1String("section")) {
                        *error = QStringLiteral("Unerwartetes Element '%1' unter <part>.")
                                     .arg(reader.name().toString());
                        return false;
                    }
                    Section section;
                    section.id = reader.attributes().value(QLatin1String("id")).toString();
                    section.title = reader.attributes().value(QLatin1String("title")).toString();
                    section.order = reader.attributes().value(QLatin1String("order")).toInt();
                    while (reader.readNextStartElement()) {
                        const QString sectionName = reader.name().toString();
                        if (sectionName == QLatin1String("chapters")) {
                            while (reader.readNextStartElement()) {
                                if (reader.name() != QLatin1String("chapter")) {
                                    *error = QStringLiteral("Unerwartetes Element '%1' unter "
                                                           "<chapters>.")
                                                 .arg(reader.name().toString());
                                    return false;
                                }
                                Chapter chapter;
                                chapter.id =
                                    reader.attributes().value(QLatin1String("id")).toString();
                                chapter.title =
                                    reader.attributes().value(QLatin1String("title")).toString();
                                chapter.order =
                                    reader.attributes().value(QLatin1String("order")).toInt();
                                while (reader.readNextStartElement()) {
                                    if (reader.name() != QLatin1String("questions")) {
                                        *error = QStringLiteral("Unerwartetes Element '%1' unter "
                                                               "<chapter>.")
                                                     .arg(reader.name().toString());
                                        return false;
                                    }
                                    while (reader.readNextStartElement()) {
                                        if (reader.name() != QLatin1String("question")) {
                                            *error = QStringLiteral("Unerwartetes Element '%1' "
                                                                   "unter <questions>.")
                                                         .arg(reader.name().toString());
                                            return false;
                                        }
                                        Question question;
                                        if (!readQuestion(reader, &question, error)) {
                                            if (error->isEmpty())
                                                *error = readerError(reader, contextName);
                                            return false;
                                        }
                                        chapter.questions.append(question);
                                    }
                                }
                                section.chapters.append(chapter);
                            }
                        } else {
                            *error = QStringLiteral("Unbekanntes Abschnitts-Element '%1'.")
                                         .arg(sectionName);
                            return false;
                        }
                    }
                    part.sections.append(section);
                }
                catalog->parts.append(part);
            }
        } else {
            *error = QStringLiteral("Unbekanntes Wurzelelement '%1'.").arg(name);
            return false;
        }
    }

    if (reader.hasError()) {
        *error = readerError(reader, contextName);
        return false;
    }
    return true;
}

bool CatalogXml::load(const QString &path, Catalog *catalog, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Datei nicht lesbar: %1").arg(file.errorString());
        return false;
    }
    const QByteArray document = file.readAll();
    file.close();
    return loadFromString(document, catalog, error, path);
}

QByteArray CatalogXml::toXml(const Catalog &catalog)
{
    QByteArray output;
    QXmlStreamWriter writer(&output);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);

    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("catalog"));
    writer.writeAttribute(QStringLiteral("schemaVersion"),
                          QString::number(catalog.schemaVersion));

    const CatalogMetadata &metadata = catalog.metadata;
    writer.writeStartElement(QStringLiteral("metadata"));
    writer.writeTextElement(QStringLiteral("title"), metadata.title);
    writer.writeTextElement(QStringLiteral("examClass"),
                            examClassToString(metadata.examClass));
    writer.writeTextElement(QStringLiteral("examId"), metadata.examId);
    writer.writeTextElement(QStringLiteral("authority"), metadata.authority);
    writer.writeTextElement(QStringLiteral("catalogVersion"), metadata.catalogVersion);
    writer.writeTextElement(QStringLiteral("validFrom"), metadata.validFrom.toString(Qt::ISODate));
    if (metadata.validUntil.isValid())
        writer.writeTextElement(QStringLiteral("validUntil"),
                                metadata.validUntil.toString(Qt::ISODate));
    writer.writeTextElement(QStringLiteral("language"), metadata.language);
    if (metadata.defaultPoints > 0)
        writer.writeTextElement(QStringLiteral("defaultPoints"),
                                QString::number(metadata.defaultPoints));
    if (!metadata.sourceUrl.isEmpty())
        writer.writeTextElement(QStringLiteral("sourceUrl"), metadata.sourceUrl);
    if (!metadata.license.isEmpty())
        writer.writeTextElement(QStringLiteral("license"), metadata.license);
    if (!metadata.attribution.isEmpty())
        writer.writeTextElement(QStringLiteral("attribution"), metadata.attribution);
    if (metadata.modifiedSinceSource)
        writer.writeTextElement(QStringLiteral("modifiedSinceSource"),
                                QStringLiteral("true"));
    if (!metadata.notes.isEmpty())
        writer.writeTextElement(QStringLiteral("notes"), metadata.notes);
    writer.writeEndElement();

    writer.writeStartElement(QStringLiteral("parts"));
    for (const Part &part : catalog.parts) {
        writer.writeStartElement(QStringLiteral("part"));
        writer.writeAttribute(QStringLiteral("id"), part.id);
        writer.writeAttribute(QStringLiteral("title"), part.title);
        writer.writeAttribute(QStringLiteral("order"), QString::number(part.order));
        if (part.selection.questions > 0) {
            writer.writeStartElement(QStringLiteral("selection"));
            writer.writeAttribute(QStringLiteral("questions"),
                                  QString::number(part.selection.questions));
            if (part.selection.errorPoints)
                writer.writeAttribute(QStringLiteral("errorPoints"),
                                      QString::number(*part.selection.errorPoints));
            if (part.selection.passScore)
                writer.writeAttribute(QStringLiteral("passScore"),
                                      QString::number(*part.selection.passScore));
            if (part.selection.minutes)
                writer.writeAttribute(QStringLiteral("minutes"),
                                      QString::number(*part.selection.minutes));
            writer.writeEndElement();
        }
        for (const Section &section : part.sections) {
            writer.writeStartElement(QStringLiteral("section"));
            writer.writeAttribute(QStringLiteral("id"), section.id);
            writer.writeAttribute(QStringLiteral("title"), section.title);
            writer.writeAttribute(QStringLiteral("order"), QString::number(section.order));
            writer.writeStartElement(QStringLiteral("chapters"));
            for (const Chapter &chapter : section.chapters) {
                writer.writeStartElement(QStringLiteral("chapter"));
                writer.writeAttribute(QStringLiteral("id"), chapter.id);
                writer.writeAttribute(QStringLiteral("title"), chapter.title);
                writer.writeAttribute(QStringLiteral("order"), QString::number(chapter.order));
                writer.writeStartElement(QStringLiteral("questions"));
                for (const Question &question : chapter.questions) {
                    writer.writeStartElement(QStringLiteral("question"));
                    writer.writeAttribute(QStringLiteral("id"), question.id);
                    writer.writeAttribute(QStringLiteral("order"), QString::number(question.order));
                    writer.writeAttribute(QStringLiteral("type"),
                                          questionTypeToString(question.type));
                    if (question.points > 0) {
                        writer.writeAttribute(QStringLiteral("points"),
                                              QString::number(question.points));
                    }
                    if (question.locked)
                        writer.writeAttribute(QStringLiteral("locked"), QStringLiteral("true"));
                    if (question.textFormat == TextFormat::Latex) {
                        writer.writeAttribute(QStringLiteral("textFormat"),
                                              QStringLiteral("latex"));
                    }
                    writer.writeTextElement(QStringLiteral("text"), question.text);
                    writer.writeStartElement(QStringLiteral("answers"));
                    for (const Answer &answer : question.answers) {
                        writer.writeEmptyElement(QStringLiteral("answer"));
                        writer.writeAttribute(QStringLiteral("id"), answer.id);
                        if (!answer.text.isEmpty())
                            writer.writeAttribute(QStringLiteral("text"), answer.text);
                        if (!answer.image.isEmpty())
                            writer.writeAttribute(QStringLiteral("image"), answer.image);
                        writer.writeAttribute(QStringLiteral("correct"),
                                              answer.correct ? QStringLiteral("true")
                                                             : QStringLiteral("false"));
                        writer.writeAttribute(QStringLiteral("order"), QString::number(answer.order));
                    }
                    writer.writeEndElement();
                    if (!question.image.isEmpty())
                        writer.writeTextElement(QStringLiteral("image"), question.image);
                    if (!question.explanation.isEmpty())
                        writer.writeTextElement(QStringLiteral("explanation"), question.explanation);
                    writer.writeStartElement(QStringLiteral("history"));
                    writer.writeTextElement(QStringLiteral("addedOn"),
                                            question.history.addedOn.toString(Qt::ISODate));
                    if (question.history.changedOn.isValid())
                        writer.writeTextElement(QStringLiteral("changedOn"),
                                                question.history.changedOn.toString(Qt::ISODate));
                    if (question.history.retiredOn.isValid())
                        writer.writeTextElement(QStringLiteral("retiredOn"),
                                                question.history.retiredOn.toString(Qt::ISODate));
                    writer.writeEndElement();
                    writer.writeEndElement();
                }
                writer.writeEndElement();   // questions
                writer.writeEndElement();   // chapter
            }
            writer.writeEndElement();       // chapters
            writer.writeEndElement();       // section
        }
        writer.writeEndElement();           // part
    }
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();
    return output;
}

bool CatalogXml::save(const QString &path, const Catalog &catalog, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = QStringLiteral("Datei nicht schreibbar: %1").arg(file.errorString());
        return false;
    }
    const QByteArray document = toXml(catalog);
    if (file.write(document) != document.size()) {
        *error = QStringLiteral("Datei nicht vollständig geschrieben: %1").arg(file.errorString());
        return false;
    }
    if (!file.commit()) {
        *error = QStringLiteral("Datei nicht geschrieben: %1").arg(file.errorString());
        return false;
    }
    return true;
}

} // namespace hrt