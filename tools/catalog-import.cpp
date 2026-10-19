// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Command line front end of the catalog import. The work itself lives in
// hrt::CatalogImporter, because the application needs the same import for
// "Fragenkatalog aktualisieren…" and two importers would sooner or later
// write different files for the same source.
#include "core/CatalogImporter.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

using namespace hrt;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hrt-catalog-import"));
    QCoreApplication::setApplicationVersion(QStringLiteral(HRT_VERSION_STRING));

    QTextStream out(stdout);
    QTextStream err(stderr);
    out.setEncoding(QStringConverter::Utf8);
    err.setEncoding(QStringConverter::Utf8);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Importiert den Fragenkatalog der Bundesnetzagentur in das "
                       "Klartext-XML-Format von HamRadioTrainer 2026."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption sourceOption(
        QStringLiteral("source"),
        QStringLiteral("Verzeichnis mit fragenkatalog3b.json und svgs/."),
        QStringLiteral("dir"));
    parser.addOption(sourceOption);
    QCommandLineOption outputOption(
        QStringLiteral("output"),
        QStringLiteral("Zielverzeichnis für die erzeugten Kataloge und Bilder."),
        QStringLiteral("dir"));
    parser.addOption(outputOption);
    QCommandLineOption sharedPartsOption(
        QStringLiteral("no-shared-parts"),
        QStringLiteral("Betriebliche Kenntnisse und Kenntnisse von Vorschriften nicht als "
                       "gemeinsamer Pool für alle Klassen behandeln."));
    parser.addOption(sharedPartsOption);
    parser.process(app);

    const QString sourceDirPath = parser.isSet(sourceOption)
                                      ? parser.value(sourceOption)
                                      : parser.positionalArguments().value(0);
    const QString outputDirPath =
        parser.isSet(outputOption) ? parser.value(outputOption) : QString();

    if (sourceDirPath.isEmpty() || outputDirPath.isEmpty()) {
        err << QStringLiteral("Bitte --source und --output angeben. Siehe --help.\n");
        return 2;
    }

    CatalogImportOptions options;
    options.sharedParts = !parser.isSet(sharedPartsOption);
    const CatalogImportReport report = CatalogImporter::importSource(sourceDirPath,
                                                                    outputDirPath, options);

    for (const CatalogImportEntry &entry : report.catalogs) {
        out << QStringLiteral("%1: %2 Fragen\n").arg(entry.fileName).arg(entry.questions);
    }
    for (const QString &warning : report.warnings)
        out << QStringLiteral("Hinweis %1\n").arg(warning);
    for (const QString &error : report.errors)
        err << QStringLiteral("FEHLER %1\n").arg(error);
    out << QStringLiteral("Bilder kopiert: %1, fehlend: %2\n")
               .arg(report.imagesCopied).arg(report.imagesMissing);

    return report.ok ? 0 : 1;
}
