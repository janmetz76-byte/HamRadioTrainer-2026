// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
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
///
/// A changed catalog is not a reason to throw the progress away: a learner who
/// has worked through two years of questions does not start over because three
/// questions were reworded. The states are therefore matched question by
/// question, and the counts say what happened to each of them.
struct ProgressLoad
{
    /// States that fit the catalog that was loaded. Empty when the file was
    /// missing, unreadable or rejected.
    QVector<QuestionState> states;

    /// Why nothing could be read, empty when it worked.
    QString error;

    /// True when the file belonged to a different revision of the catalog.
    /// The states are applied, but the caller should tell the learner what
    /// happened: see keptStates, resetStates and droppedStates.
    bool catalogChanged = false;

    /// States applied unchanged: same question id, same content.
    int keptStates = 0;

    /// States whose question still exists by id but no longer reads the same.
    /// They are applied as "never answered": the learner has not seen this
    /// wording, and crediting the old answer would be a lie.
    int resetStates = 0;

    /// States referring to questions the catalog no longer contains.
    int droppedStates = 0;

    /// The file predates the per question fingerprint, so "kept" could only be
    /// decided by the question id. Worth saying out loud, because it is the
    /// difference between "same question" and "same number".
    bool withoutFingerprints = false;
};

/// Reads and writes the learning progress of one catalog.
///
/// The progress is kept outside the catalog itself, because the catalogs are
/// meant to be edited by hand and are part of the distribution.
///
/// The file names the digest of the catalog it was written for, so a changed
/// catalog is noticed. It does not decide what happens: each state carries a
/// fingerprint of its question, so an unchanged catalog applies everything and
/// a changed one applies the questions that still read the same. The rest is
/// counted and reported rather than discarded.
class ProgressStore
{
public:
    /// Format marker written into every file, so a foreign JSON file is never
    /// mistaken for progress.
    inline static const QString kFormat = QStringLiteral("hrt-progress");
    /// Bumped whenever the layout changes in an incompatible way. Version 1 had
    /// no per question fingerprint and is still readable.
    static constexpr int kVersion = 2;
    /// The oldest layout that can still be read.
    static constexpr int kOldestReadableVersion = 1;

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

    /// Copies the current file to "<path>.bak" before it is rebound to a new
    /// catalog. Whatever the learner then chooses, the previous state of the
    /// file is still on disk.
    bool archive(QString *error) const;

private:
    QString m_path;
};

} // namespace hrt