// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "learning/Scheduler.h"

#include <QString>
#include <QVector>

namespace hrt {

/// Outcome of reading a progress file.
struct ProgressLoad
{
    /// States that fit the catalog that was loaded. Empty when the file was
    /// missing, unreadable or rejected.
    QVector<QuestionState> states;

    /// Why nothing could be read, empty when it worked.
    QString error;

    /// True when a file existed and was well formed, but belonged to a
    /// different revision of the catalog. The states are then not applied,
    /// because questions may have been reworded, replaced or removed by hand.
    bool discardedDueToCatalogChange = false;

    /// States referring to questions the catalog no longer contains.
    int droppedStates = 0;
};

/// Reads and writes the learning progress of one catalog.
///
/// The progress is kept outside the catalog itself, because the catalogs are
/// meant to be edited by hand and are part of the distribution. It is bound to
/// the digest of the catalog file it was written for: once the file changes,
/// old progress is discarded instead of being applied to questions that may no
/// longer look the same.
class ProgressStore
{
public:
    /// Format marker written into every file, so a foreign JSON file is never
    /// mistaken for progress.
    inline static const QString kFormat = QStringLiteral("hrt-progress");
    /// Bumped whenever the layout changes in an incompatible way.
    static constexpr int kVersion = 1;

    explicit ProgressStore(QString path);

    /// Where the progress of a catalog belongs by default: in the per user
    /// application data directory, named after the exam id.
    static QString defaultPath(const Catalog &catalog);

    const QString &path() const { return m_path; }

    /// Reads the progress. Never fails hard: a missing file simply yields an
    /// empty progress, which is the normal first start.
    ProgressLoad load(const Catalog &catalog) const;

    /// Writes the states of questions the catalog contains. The file is
    /// replaced atomically, so a crash cannot leave a half written progress.
    bool save(const Catalog &catalog, const QVector<QuestionState> &states,
              QString *error) const;

    /// Replaces the progress file with an empty one, keeping the binding to the
    /// current catalog.
    bool clear(const Catalog &catalog, QString *error) const;

private:
    QString m_path;
};

} // namespace hrt