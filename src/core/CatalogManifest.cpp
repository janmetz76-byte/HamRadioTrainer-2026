// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "core/CatalogManifest.h"

#include "core/Catalog.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace hrt {

namespace {

QString field(const QJsonObject &object, const char *name)
{
    return object.value(QLatin1String(name)).toString();
}

} // namespace

CatalogManifest CatalogManifest::load(const QString &directory)
{
    CatalogManifest manifest;
    QFile file(QDir(directory).filePath(QStringLiteral("catalog-manifest.json")));
    if (!file.open(QIODevice::ReadOnly))
        return manifest;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return manifest;
    const QJsonArray catalogs =
        document.object().value(QStringLiteral("catalogs")).toArray();
    for (const QJsonValue &value : catalogs) {
        const QJsonObject object = value.toObject();
        CatalogManifestEntry entry;
        entry.examClass = field(object, "examClass");
        entry.file = field(object, "file");
        entry.sha256 = field(object, "sha256");
        entry.questions = object.value(QStringLiteral("questions")).toInt();
        entry.images = object.value(QStringLiteral("images")).toInt();
        entry.catalogVersion = field(object, "catalogVersion");
        entry.validFrom = QDate::fromString(field(object, "validFrom"), Qt::ISODate);
        if (!entry.examClass.isEmpty())
            manifest.m_entries.append(entry);
    }
    return manifest;
}

CatalogManifestEntry CatalogManifest::entryFor(const Catalog &catalog) const
{
    const QString wanted = examClassToString(catalog.metadata.examClass);
    for (const CatalogManifestEntry &entry : m_entries) {
        if (entry.examClass == wanted)
            return entry;
    }
    return {};
}

CatalogDrift CatalogManifest::driftOf(const Catalog &catalog, const QString &directory) const
{
    Q_UNUSED(directory);
    if (m_entries.isEmpty())
        return CatalogDrift::Unknown;

    const CatalogManifestEntry entry = entryFor(catalog);
    if (entry.sha256.isEmpty())
        return CatalogDrift::Unknown;
    if (!catalog.sourceDigest.isEmpty() && catalog.sourceDigest == entry.sha256)
        return CatalogDrift::AsShipped;
    return CatalogDrift::Edited;
}

QString CatalogManifest::describe(CatalogDrift drift, const Catalog &catalog,
                                  const CatalogManifestEntry &entry)
{
    const QString stand = entry.catalogVersion.isEmpty()
                              ? QStringLiteral("unbekannte Auflage")
                              : QStringLiteral("%1, gültig ab %2")
                                    .arg(entry.catalogVersion,
                                         entry.validFrom.isValid()
                                             ? entry.validFrom.toString(QStringLiteral("dd.MM.yyyy"))
                                             : QStringLiteral("unbekannt"));

    switch (drift) {
    case CatalogDrift::AsShipped:
        return QObject::tr("Entspricht dem ausgelieferten Stand: %1, %2 Fragen.")
            .arg(stand)
            .arg(catalog.activeQuestionCount());
    case CatalogDrift::Edited:
        return QObject::tr(
                   "Von Hand geändert: der ausgelieferte Stand war %1 mit %2 Fragen. "
                   "Beim Aktualisieren aus der Quelle der Bundesnetzagentur werden diese "
                   "Änderungen überschrieben.")
            .arg(stand)
            .arg(entry.questions);
    case CatalogDrift::Newer:
        return QObject::tr("Neuere Auflage als ausgeliefert: %1, %2 Fragen.")
            .arg(stand)
            .arg(catalog.activeQuestionCount());
    case CatalogDrift::Unknown:
        break;
    }
    return QObject::tr("Für diese Klasse liegt kein Sollstand bei; das Programm kann nicht "
                       "sagen, ob der Katalog verändert wurde.");
}

} // namespace hrt