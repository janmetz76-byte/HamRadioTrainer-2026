// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "exam/ExamSession.h"
#include "learning/Scheduler.h"

#include <QElapsedTimer>
#include <QSet>
#include <QWidget>

#include <optional>

class QGridLayout;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTimer;
class QVBoxLayout;

namespace hrt {

class AnswerView;

/// The examination view.
///
/// It draws a paper either as a real examination, which reproduces the scope
/// of the Amtsblatt-Verfügung 29/2024, or as a training paper of any length.
/// Grading lives in ExamSession, this class only presents the questions and
/// collects the clicks.
class ExamWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExamWidget(QWidget *parent = nullptr);

    /// Hands the loaded catalog over. Any running examination is discarded,
    /// because its questions come from the previous catalog.
    void setCatalog(const Catalog *catalog, const QString &catalogPath);

    /// Continues with the learning progress read from disk.
    void setProgress(const QVector<QuestionState> &states);

    /// Forget everything that was learned so far, e.g. after the catalog was
    /// replaced by a new edition.
    void clearProgress();

    /// Questions of the official examination and its time limit in minutes.
    bool startExam();

    /// Restricts the simulated exam to the chosen parts.
    ///
    /// The Amtsblatt-Verfügung only allows V+B+N, V+B+N+E and all five parts.
    /// An empty set means "all parts", which is what a learner gets who never
    /// used the start assistant.
    void setSelectedParts(const QSet<int> &parts);

    /// Narrow layout for a phone in portrait: start and navigation controls
    /// are stacked instead of sharing one line.
    void setCompact(bool compact);
    QSet<int> selectedParts() const { return m_selectedParts; }
    /// A training paper of the given length, without pass marks.
    bool startTraining(int questionCount);

    /// Learning progress collected by finished papers, to be persisted.
    const Scheduler &scheduler() const { return m_scheduler; }

signals:
    /// Emitted after a finished paper, with the progress that has to be kept.
    void progressChanged(const hrt::Scheduler &scheduler);

    /// Emitted once the paper has been graded. The widget does not present the
    /// result itself, so the application decides how to show it.
    void examFinished(const hrt::ExamResult &result);

private slots:
    void showCurrentQuestion();
    void goToNextUnanswered();
    void goBack();
    void finishExam();
    void tick();

private:
    void arrangeControls();
    void buildUi();
    void updateHeader();
    void updateNavigation();
    QPixmap renderImage(const QString &relativePath) const;
    void applyResultToScheduler(const ExamResult &result);

    const Catalog *m_catalog = nullptr;
    QString m_catalogDir;
    std::optional<ExamSession> m_session;
    Scheduler m_scheduler;
    int m_current = 0;

    QElapsedTimer m_clock;
    QTimer *m_timer = nullptr;

    QLabel *m_header = nullptr;
    QLabel *m_timerLabel = nullptr;
    QLabel *m_progress = nullptr;
    AnswerView *m_answers = nullptr;

    QPushButton *m_startExam = nullptr;
    QPushButton *m_startTraining = nullptr;
    QSet<int> m_selectedParts;
    bool m_compact = false;
    QGridLayout *m_startGrid = nullptr;
    QGridLayout *m_navGrid = nullptr;
    QLabel *m_trainingLabel = nullptr;
    QSpinBox *m_trainingCount = nullptr;
    QPushButton *m_back = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_finish = nullptr;
};

} // namespace hrt