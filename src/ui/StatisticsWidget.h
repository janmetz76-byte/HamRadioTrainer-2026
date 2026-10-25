// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include "core/Catalog.h"
#include "learning/LearningStatistics.h"
#include "learning/Scheduler.h"

#include <QWidget>

class QLabel;
class QTableWidget;

namespace hrt {

/// Shows where the learner stands: overall figures, the spread over the
/// learning stages and the figures per Prüfungsteil.
///
/// The learning view answers "what comes next" - it works through the due
/// questions. This view answers "where do I stand", which is a different
/// question and a different reading: nobody plans the day by it, but it is
/// what tells whether the plan works at all.
///
/// It therefore does not repeat the stage row of the learning view. It shows
/// the same numbers arranged so they can be compared: stage against stage,
/// part against part.
class StatisticsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StatisticsWidget(QWidget *parent = nullptr);

    void setCatalog(const Catalog *catalog, const QString &catalogPath);
    void setProgress(const QVector<QuestionState> &states);
    /// Prüfungsteile to count, as indexes into the catalog's part list. Empty
    /// counts the whole catalog.
    void setSelectedParts(const QSet<int> &parts);

    /// Recomputes the figures from the stored progress.
    void refresh();

private:
    void clear();
    void updateOverall();
    void updateStages();
    void updateParts();

    const Catalog *m_catalog = nullptr;
    Scheduler m_scheduler;
    QSet<int> m_selectedParts;
    LearningStatistics m_statistics;

    QLabel *m_overall = nullptr;
    QTableWidget *m_stages = nullptr;
    QTableWidget *m_parts = nullptr;
};

} // namespace hrt