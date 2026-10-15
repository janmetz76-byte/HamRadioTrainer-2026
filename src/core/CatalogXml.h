// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include "Catalog.h"

#include <QString>

namespace hrt {

/// Reads and writes the plain XML catalog format described by
/// schema/catalog.xsd. No encryption, no compression, no obfuscation is applied
/// in either direction; the file stays editable by hand.
class CatalogXml
{
public:
    /// Parses a catalog from disk. On failure returns false and fills "error"
    /// with a message that names the offending line.
    static bool load(const QString &path, Catalog *catalog, QString *error);

    /// Parses a catalog from an in-memory document. Used by the tests and by
    /// drag and drop.
    static bool loadFromString(const QByteArray &document,
                               Catalog *catalog,
                               QString *error,
                               const QString &contextName = QString());

    /// Writes the catalog. The output is deterministic: elements are written in
    /// document order, not in hash order, so that repeated saves of unchanged
    /// data produce byte identical files and empty git diffs.
    static bool save(const QString &path, const Catalog &catalog, QString *error);

    static QByteArray toXml(const Catalog &catalog);
};

} // namespace hrt