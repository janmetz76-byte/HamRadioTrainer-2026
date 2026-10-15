// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Command line companion for maintaining question catalogs: it lets a catalog
// be checked, inspected and normalised in a build script or in CI without
// starting the graphical application.
#include "core/CatalogValidator.h"
#include "core/CatalogXml.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

using namespace hrt;

namespace {

int report(const QStringList &paths, bool verbose)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    int errors = 0;
    int warnings = 0;

    for (const QString &path : paths) {
        Catalog catalog;
        QString loadError;
        if (!CatalogXml::load(path, &catalog, &loadError)) {
            err << QStringLiteral("%1: %2\n").arg(path, loadError);
            ++errors;
            continue;
        }

        QVector<CatalogIssue> issues;
        const bool usable = CatalogValidator::validate(catalog, &issues, path);

        if (verbose) {
            out << QStringLiteral("%1\n").arg(path);
            out << QStringLiteral("  Klasse:        %1\n")
                       .arg(examClassToString(catalog.metadata.examClass));
            out << QStringLiteral("  Prüfung:       %1\n").arg(catalog.metadata.examId);
            out << QStringLiteral("  Katalogstand:  %1\n").arg(catalog.metadata.catalogVersion);
            out << QStringLiteral("  Gültig ab:     %1\n")
                       .arg(catalog.metadata.validFrom.toString(Qt::ISODate));
            out << QStringLiteral("  Teile:         %1\n").arg(catalog.parts.size());
            out << QStringLiteral("  Fragen gesamt: %1\n").arg(catalog.totalQuestionCount());
            out << QStringLiteral("  Fragen aktiv:  %1\n").arg(catalog.activeQuestionCount());
        }

        for (const CatalogIssue &issue : issues) {
            const bool isError = issue.severity == IssueSeverity::Error;
            isError ? ++errors : ++warnings;
            err << QStringLiteral("%1: %2 %3 — %4\n")
                       .arg(path, isError ? QStringLiteral("FEHLER") : QStringLiteral("HINWEIS"),
                            issue.location, issue.message);
        }

        if (!usable && verbose)
            err << QStringLiteral("%1: nicht verwendbar\n").arg(path);
    }

    out << QStringLiteral("%1 Fehler, %2 Hinweise\n").arg(errors).arg(warnings);
    return errors > 0 ? 1 : 0;
}

int normalize(const QStringList &paths, bool checkOnly)
{
    QTextStream out(stdout);
    QTextStream err(stderr);
    int problems = 0;

    for (const QString &path : paths) {
        Catalog catalog;
        QString error;
        if (!CatalogXml::load(path, &catalog, &error)) {
            err << QStringLiteral("%1: %2\n").arg(path, error);
            ++problems;
            continue;
        }

        const QByteArray normalized = CatalogXml::toXml(catalog);

        if (checkOnly) {
            QFile original(path);
            if (!original.open(QIODevice::ReadOnly)) {
                err << QStringLiteral("%1: %2\n").arg(path, original.errorString());
                ++problems;
                continue;
            }
            if (original.readAll() != normalized) {
                out << QStringLiteral("nicht normalisiert: %1\n").arg(path);
                ++problems;
            } else {
                out << QStringLiteral("normalisiert: %1\n").arg(path);
            }
            continue;
        }

        const QString target = QDir(path).absoluteFilePath(path);
        QString saveError;
        if (!CatalogXml::save(target, catalog, &saveError)) {
            err << QStringLiteral("%1: %2\n").arg(path, saveError);
            ++problems;
            continue;
        }
        out << QStringLiteral("geschrieben: %1\n").arg(path);
    }

    return problems > 0 ? 1 : 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hrt-catalog-tool"));
    QCoreApplication::setApplicationVersion(QStringLiteral(HRT_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Prueft, inspiziert und normalisiert HamRadioTrainer 2026 "
                       "Fragenkataloge."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption infoOption(QStringLiteral("info"),
                                  QStringLiteral("Kennzahlen je Katalog ausgeben."));
    parser.addOption(infoOption);
    QCommandLineOption normalizeOption(
        QStringLiteral("normalize"),
        QStringLiteral("Katalog lesen und im kanonischen Format neu schreiben. Ohne --write "
                       "wird nur geprüft und nichts verändert."));
    parser.addOption(normalizeOption);
    QCommandLineOption writeOption(QStringLiteral("write"),
                                   QStringLiteral("Mit --normalize tatsächlich schreiben."));
    parser.addOption(writeOption);

    parser.addPositionalArgument(QStringLiteral("files"),
                                 QStringLiteral("Fragenkataloge (XML)."),
                                 QStringLiteral("files..."));

    parser.process(app);

    const QStringList files = parser.positionalArguments();
    if (files.isEmpty()) {
        QTextStream(stderr) << QStringLiteral("Keine Datei angegeben. Siehe --help.\n");
        return 2;
    }

    if (parser.isSet(normalizeOption))
        return normalize(files, !parser.isSet(writeOption));

    return report(files, parser.isSet(infoOption) || files.size() > 1);
}