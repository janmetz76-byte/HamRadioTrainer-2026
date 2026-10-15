// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "exam/PaperBuilder.h"
#include "learning/LearningStatistics.h"
#include "learning/Scheduler.h"
#include "learning/StudySession.h"

#include <QHash>
#include <QWidget>

#include <optional>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace hrt {

class AnswerView;

/// The learning view: where the learner stands, and a practice round that
/// works through the weak questions first.
///
/// The self report is the point of the practice round. An examination cannot
/// ask how sure the learner felt, so it records a neutral result; only here the
/// confidence is collected, and that is what makes a question answered while
/// guessing come back sooner than one that was known.
class LearningWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LearningWidget(QWidget *parent = nullptr);

    void setCatalog(const Catalog *catalog, const QString &catalogPath);
    void setProgress(const QVector<QuestionState> &states);
    void clearProgress();
    const Scheduler &scheduler() const { return m_scheduler; }

    /// Recomputes the figures from the stored progress.
    void refresh();

    /// Id of the question that is currently shown, empty when no round is
    /// running. Lets a caller tell which question the learner is working on.
    QString currentQuestionId() const { return m_pendingQuestionId; }
    /// The plan of the running round, empty when none is running.
    QStringList currentPlan() const
    {
        return m_session ? m_session->plan() : QStringList();
    }

signals:
    void progressChanged(const hrt::Scheduler &scheduler);

private slots:
    void startRound();
    void showCurrent();
    void onAnswerSelected(int position);
    void rateConfidence(int confidence);
    void showSummary();

private:
    StudyOptions currentOptions() const;
    void refreshPartFilter();
    void updateSummary();

    const Catalog *m_catalog = nullptr;
    QString m_catalogDir;
    Scheduler m_scheduler;
    QHash<QString, const Question *> m_byId;

    std::optional<StudySession> m_session;
    int m_index = 0;
    int m_correct = 0;
    quint32 m_seed = 0;
    QString m_pendingQuestionId;
    int m_pendingCorrectPosition = -1;

    QLabel *m_summary = nullptr;
    QTableWidget *m_boxes = nullptr;
    QTableWidget *m_parts = nullptr;
    QLabel *m_progress = nullptr;

    QSpinBox *m_roundSize = nullptr;
    QCheckBox *m_onlyDue = nullptr;
    QCheckBox *m_onlyShaky = nullptr;
    QComboBox *m_partFilter = nullptr;
    QPushButton *m_start = nullptr;

    AnswerView *m_answers = nullptr;
    QLabel *m_explanation = nullptr;
    QPushButton *m_confident = nullptr;
    QPushButton *m_unsure = nullptr;
    QPushButton *m_guessing = nullptr;
    QPushButton *m_nextQuestion = nullptr;
};

} // namespace hrt