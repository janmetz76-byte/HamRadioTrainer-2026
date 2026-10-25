// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include <QDate>
#include <QString>
#include <QVector>

namespace hrt {

struct Catalog;

/// What the shipped manifest says about one class.
struct CatalogManifestEntry
{
    QString examClass;
    QString file;
    QString sha256;
    int questions = 0;
    int images = 0;
    QString catalogVersion;
    QDate validFrom;
};

/// The state a loaded catalog is in, compared with the shipped manifest.
enum class CatalogDrift {
    Unknown,        ///< no manifest, or no entry for this class
    AsShipped,      ///< byte for byte the delivered catalog
    Edited,         ///< usable, but changed by hand since the import
    Newer,          ///< changed by a newer import than the one delivered
};

/// The reference state of the delivered catalogs.
///
/// Answers the question "is this still the catalog we shipped?": the program
/// can only detect that the file no longer matches, never what the
/// Bundesnetzagentur has published in the meantime - that needs their
/// archive, which the learner downloads.
class CatalogManifest
{
public:
    /// Reads catalog-manifest.json. An unreadable or missing file is not an
    /// error: the program then simply cannot tell, and says so.
    static CatalogManifest load(const QString &directory);

    bool isEmpty() const { return m_entries.isEmpty(); }
    QVector<CatalogManifestEntry> entries() const { return m_entries; }

    /// Compares a loaded catalog with the entry of its class.
    CatalogDrift driftOf(const Catalog &catalog, const QString &directory) const;

    /// One sentence for the report or the status bar.
    static QString describe(CatalogDrift drift, const Catalog &catalog,
                            const CatalogManifestEntry &entry);

private:
    CatalogManifestEntry entryFor(const Catalog &catalog) const;

    QVector<CatalogManifestEntry> m_entries;
};

} // namespace hrt