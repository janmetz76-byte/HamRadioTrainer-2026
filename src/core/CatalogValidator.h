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
#include <QVector>

namespace hrt {

enum class IssueSeverity {
    Error,   ///< The catalog cannot be used for exam generation.
    Warning, ///< Usable, but suspicious or in need of review.
};

struct CatalogIssue
{
    IssueSeverity severity = IssueSeverity::Error;
    QString location;   ///< Id of the offending part, section, chapter or question.
    QString message;    ///< German, user facing.
};

/// Semantic validation on top of schema/catalog.xsd. The XSD covers shape and
/// identifier syntax; this covers everything a schema cannot express, such as
/// "at least one answer has to be correct" or "a section cannot ask for more
/// questions than it contains".
class CatalogValidator
{
public:
    /// Appends every issue found. Returns true when no error level issue was
    /// found, i.e. when the catalog is usable.
    static bool validate(const Catalog &catalog,
                         QVector<CatalogIssue> *issues,
                         const QString &basePath = QString());

private:
    static void checkSection(const Section &section,
                             const QString &basePath,
                             const CatalogMetadata &metadata,
                             QVector<CatalogIssue> *issues,
                             QStringList *seenIds);
    static void checkQuestion(const Question &question,
                              const QString &chapterId,
                              const QString &basePath,
                              const CatalogMetadata &metadata,
                              QVector<CatalogIssue> *issues,
                              QStringList *seenIds);
};

} // namespace hrt