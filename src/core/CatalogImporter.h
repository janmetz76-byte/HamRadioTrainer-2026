// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace hrt {

/// How the shared sheets V and B are treated.
///
/// The Bundesnetzagentur keeps them once for all classes; a candidate who has
/// already passed class N does not answer them again for class E.
struct CatalogImportOptions
{
    bool sharedParts = true;
};

/// What one class produced.
struct CatalogImportEntry
{
    QString fileName;        ///< de-amateurfunk-N.xml
    QString path;            ///< full path of the written file
    int questions = 0;
    /// The file content before this import, empty when there was none. The
    /// caller decides what to do with it - the application keeps a backup,
    /// the command line tool does not.
    QByteArray previousContent;
};

/// The outcome of a whole import: three classes and the images.
///
/// Errors mean the result must not be used: a catalog without a picture that
/// a question refers to is unusable in the learning view, and a missing
/// license notice makes the redistribution unlawful.
struct CatalogImportReport
{
    bool ok = false;
    QVector<CatalogImportEntry> catalogs;
    QStringList errors;
    QStringList warnings;
    int imagesCopied = 0;
    int imagesMissing = 0;
};

/// Import of the machine readable question catalog of the Bundesnetzagentur.
///
/// Lives in the library and not only in the command line tool: the
/// application needs it for "Fragenkatalog aktualisieren…", and two copies
/// of an importer would sooner or later write different files for the same
/// source.
///
/// The question text is not covered by the GPL. It is redistributed under
/// DL-DE-BY-2.0, which requires the verbatim source notice; the importer
/// writes that notice into the metadata of every generated catalog.
class CatalogImporter
{
public:
    /// Whether the directory holds something that looks like the archive from
    /// the Bundesnetzagentur: fragenkatalog3b.json with a metadata section.
    ///
    /// Meant for the file dialog of the application, before anything is
    /// written. Explains what is missing instead of only saying "no".
    static bool sourceIsUsable(const QString &sourceDir, QString *error = nullptr);

    /// Reads fragenkatalog3b.json and writes one catalog per class plus the
    /// images into outputDir, replacing what was there.
    static CatalogImportReport importSource(const QString &sourceDir,
                                            const QString &outputDir,
                                            const CatalogImportOptions &options = {});
};

} // namespace hrt