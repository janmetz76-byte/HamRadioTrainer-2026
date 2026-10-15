// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ProgressStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace hrt {
namespace {

/// A question id that the catalog does not contain any more must not keep a
/// state, otherwise the weighting would grow entries nobody can ever reach.
QStringList knownQuestionIds(const Catalog &catalog)
{
    QStringList ids;
    for (const Part &part : catalog.parts) {
        for (const Section &section : part.sections) {
            for (const Question *question : section.questions())
                ids.append(question->id);
        }
    }
    return ids;
}

QString dateToString(const QDate &date)
{
    return date.isValid() ? date.toString(Qt::ISODate) : QString();
}

QDate dateFromString(const QString &text)
{
    return text.isEmpty() ? QDate() : QDate::fromString(text, Qt::ISODate);
}

} // namespace

ProgressStore::ProgressStore(QString path)
    : m_path(std::move(path))
{
}

QString ProgressStore::defaultPath(const Catalog &catalog)
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString name = catalog.metadata.examId.isEmpty()
                             ? QStringLiteral("katalog")
                             : catalog.metadata.examId.toLower();
    return QDir(base).filePath(QStringLiteral("fortschritt-%1.json").arg(name));
}

ProgressLoad ProgressStore::load(const Catalog &catalog) const
{
    ProgressLoad result;

    QFile file(m_path);
    if (!file.exists())
        return result; // a first start is not an error
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QCoreApplication::translate("ProgressStore", "Fortschrittsdatei nicht lesbar: %1").arg(file.errorString());
        return result;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = QCoreApplication::translate("ProgressStore", "Fortschrittsdatei ist kein gültiges JSON: %1").arg(parseError.errorString());
        return result;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != kFormat) {
        result.error = QCoreApplication::translate("ProgressStore", "Fortschrittsdatei gehört nicht zu HamRadioTrainer 2026.");
        return result;
    }
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version != kVersion) {
        result.error = QCoreApplication::translate("ProgressStore", "Fortschrittsdatei hat die Version %1, erwartet wird %2.")
                           .arg(version)
                           .arg(kVersion);
        return result;
    }
    if (root.value(QStringLiteral("examId")).toString() != catalog.metadata.examId) {
        result.error = QCoreApplication::translate("ProgressStore", "Fortschrittsdatei gehört zu einem anderen Katalog (%1).")
                           .arg(root.value(QStringLiteral("examId")).toString());
        return result;
    }

    // The catalogs are meant to be edited by hand. Progress from an earlier
    // revision must not be applied to questions that may have been reworded,
    // replaced or removed since.
    if (root.value(QStringLiteral("catalogSha256")).toString() != catalog.sourceDigest) {
        result.discardedDueToCatalogChange = true;
        return result;
    }

    const QStringList known = knownQuestionIds(catalog);
    const QJsonArray states = root.value(QStringLiteral("states")).toArray();
    for (const QJsonValue &value : states) {
        const QJsonObject entry = value.toObject();
        QuestionState state;
        state.questionId = entry.value(QStringLiteral("id")).toString();
        if (state.questionId.isEmpty() || !known.contains(state.questionId)) {
            ++result.droppedStates;
            continue;
        }
        state.box = qBound(0, entry.value(QStringLiteral("box")).toInt(), Scheduler::MaxBox);
        state.timesAnswered = qMax(0, entry.value(QStringLiteral("answered")).toInt());
        state.timesCorrect = qMax(0, entry.value(QStringLiteral("correct")).toInt());
        state.consecutiveCorrect = qMax(0, entry.value(QStringLiteral("streak")).toInt());
        state.lastSeen = dateFromString(entry.value(QStringLiteral("lastSeen")).toString());
        state.dueOn = dateFromString(entry.value(QStringLiteral("dueOn")).toString());
        const int confidence = entry.value(QStringLiteral("confidence")).toInt(-1);
        state.confidence = confidence >= int(Confidence::Guessing) && confidence <= int(Confidence::Certain)
                               ? static_cast<Confidence>(confidence)
                               : Confidence::Unknown;
        result.states.append(state);
    }

    return result;
}

bool ProgressStore::save(const Catalog &catalog, const QVector<QuestionState> &states,
                         QString *error) const
{
    const QStringList known = knownQuestionIds(catalog);

    QJsonArray array;
    for (const QuestionState &state : states) {
        if (!known.contains(state.questionId))
            continue; // a question that no longer exists keeps no state
        QJsonObject entry;
        entry.insert(QStringLiteral("id"), state.questionId);
        entry.insert(QStringLiteral("box"), state.box);
        entry.insert(QStringLiteral("answered"), state.timesAnswered);
        entry.insert(QStringLiteral("correct"), state.timesCorrect);
        entry.insert(QStringLiteral("streak"), state.consecutiveCorrect);
        entry.insert(QStringLiteral("lastSeen"), dateToString(state.lastSeen));
        entry.insert(QStringLiteral("dueOn"), dateToString(state.dueOn));
        entry.insert(QStringLiteral("confidence"), int(state.confidence));
        array.append(entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QString(kFormat));
    root.insert(QStringLiteral("version"), kVersion);
    root.insert(QStringLiteral("examId"), catalog.metadata.examId);
    root.insert(QStringLiteral("catalogSha256"), catalog.sourceDigest);
    root.insert(QStringLiteral("savedOn"), QDate::currentDate().toString(Qt::ISODate));
    root.insert(QStringLiteral("states"), array);

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);

    // On a first start the application data directory does not exist yet.
    const QString directory = QFileInfo(m_path).absolutePath();
    if (!QDir().mkpath(directory)) {
        if (error)
            *error = QCoreApplication::translate(
                "ProgressStore", "Verzeichnis für den Fortschritt nicht anlegbar: %1").arg(directory);
        return false;
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QCoreApplication::translate("ProgressStore", "Fortschrittsdatei nicht schreibbar: %1").arg(file.errorString());
        return false;
    }
    if (file.write(payload) != payload.size()) {
        if (error)
            *error = QCoreApplication::translate("ProgressStore", "Fortschrittsdatei unvollständig geschrieben: %1").arg(file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error)
            *error = QCoreApplication::translate("ProgressStore", "Fortschrittsdatei nicht ersetzt: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool ProgressStore::clear(const Catalog &catalog, QString *error) const
{
    return save(catalog, {}, error);
}

} // namespace hrt