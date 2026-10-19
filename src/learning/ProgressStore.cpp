// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ProgressStore.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace hrt {
namespace {

/// What the progress remembers about a question, compressed into one word.
///
/// Only the content that decides whether an answer counts: the wording and the
/// answers with their correctness. The id is the key, not the content, and the
/// image paths are derived from the id by the import, so including them would
/// only make the fingerprint jump without a change worth learning about.
QString questionFingerprint(const Question &question)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(question.text.toUtf8());
    hash.addData("\n--\n");
    for (const Answer &answer : question.answers) {
        hash.addData(answer.text.toUtf8());
        hash.addData(answer.correct ? "\1" : "\0");
        hash.addData("\n");
    }
    return QString::fromLatin1(hash.result().toHex().left(16));
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
    if (version < kOldestReadableVersion || version > kVersion) {
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

    // Ein anderer Katalogeinstand heisst nicht, dass alles weg darf. Er heisst
    // nur, dass jede Frage fuer sich geprueft werden muss.
    result.catalogChanged =
        root.value(QStringLiteral("catalogSha256")).toString() != catalog.sourceDigest;

    const QHash<QString, const Question *> known = questionIndex(catalog);
    const QJsonArray states = root.value(QStringLiteral("states")).toArray();
    for (const QJsonValue &value : states) {
        const QJsonObject entry = value.toObject();
        QuestionState state;
        state.questionId = entry.value(QStringLiteral("id")).toString();
        const auto it = known.constFind(state.questionId);
        if (state.questionId.isEmpty() || it == known.constEnd()) {
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

        const QString fingerprint = entry.value(QStringLiteral("hash")).toString();
        if (fingerprint.isEmpty()) {
            // Datei aus der Zeit vor den Fingerabdruecken. Die Kennung ist
            // das Einzige, was wir haben - das wird dem Lernenden auch so
            // gesagt, statt es zu verschweigen.
            result.withoutFingerprints = true;
            ++result.keptStates;
        } else if (result.catalogChanged
                   && fingerprint != questionFingerprint(*it.value())) {
            // Gleiche Kennung, anderer Wortlaut. Die Antwort von damals gilt
            // fuer eine Frage, die es so nicht mehr gibt. Sie wird als
            // "noch nie beantwortet" uebernommen: das ist ehrlicher, als dem
            // Lernenden eine sichere Antwort unterzuschieben.
            const QString id = state.questionId;
            state = QuestionState{};
            state.questionId = id;
            ++result.resetStates;
        } else {
            ++result.keptStates;
        }
        result.states.append(state);
    }

    return result;
}

bool ProgressStore::save(const Catalog &catalog, const QVector<QuestionState> &states,
                         QString *error) const
{
    const QHash<QString, const Question *> known = questionIndex(catalog);

    QJsonArray array;
    for (const QuestionState &state : states) {
        // Eine Frage, die es nicht mehr gibt, darf keinen Stand behalten: sonst
        // waechst die Gewichtung um Eintraege, die niemand erreichen kann.
        const auto it = known.constFind(state.questionId);
        if (it == known.constEnd())
            continue;

        QJsonObject entry;
        entry.insert(QStringLiteral("id"), state.questionId);
        // Der Fingerabdruck erlaubt es, beim naechsten Katalogwechsel zu
        // unterscheiden, was unveraendert blieb und was neu geschrieben wurde.
        entry.insert(QStringLiteral("hash"), questionFingerprint(*it.value()));
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

bool ProgressStore::archive(QString *error) const
{
    // Solange es keine Fortschrittsdatei gibt, gibt es nichts zu sichern.
    if (!QFile::exists(m_path))
        return true;

    const QString target = m_path + QStringLiteral(".bak");
    QFile::remove(target);
    if (!QFile::copy(m_path, target)) {
        if (error) {
            *error = QCoreApplication::translate(
                         "ProgressStore", "Fortschritt nicht sicherbar: %1").arg(target);
        }
        return false;
    }
    return true;
}

bool ProgressStore::clear(const Catalog &catalog, QString *error) const
{
    return save(catalog, {}, error);
}

} // namespace hrt