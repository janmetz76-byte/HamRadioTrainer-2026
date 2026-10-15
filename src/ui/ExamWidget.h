// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "exam/ExamSession.h"
#include "learning/Scheduler.h"

#include <QElapsedTimer>
#include <QWidget>

#include <optional>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTimer;
class QVBoxLayout;

namespace hrt {

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

    /// Questions of the official examination and its time limit in minutes.
    bool startExam();
    /// A training paper of the given length, without pass marks.
    bool startTraining(int questionCount);

    /// Learning progress collected by finished papers, to be persisted.
    const Scheduler &scheduler() const { return m_scheduler; }

private slots:
    void showCurrentQuestion();
    void goToNextUnanswered();
    void goBack();
    void finishExam();
    void tick();

private:
    void buildUi();
    void clearQuestionArea();
    void activateAnswerArea();
    void updateHeader();
    void updateNavigation();
    QPixmap renderImage(const QString &relativePath) const;
    void applyResultToScheduler(const ExamResult &result);
    void showResult(const ExamResult &result);

    const Catalog *m_catalog = nullptr;
    QString m_catalogDir;
    std::optional<ExamSession> m_session;
    Scheduler m_scheduler;
    int m_current = 0;

    QElapsedTimer m_clock;
    QTimer *m_timer = nullptr;

    QLabel *m_header = nullptr;
    QLabel *m_timerLabel = nullptr;
    QLabel *m_question = nullptr;
    QLabel *m_progress = nullptr;
    QWidget *m_answerArea = nullptr;
    QVBoxLayout *m_answerLayout = nullptr;
    QButtonGroup *m_singleChoice = nullptr;
    QVector<QCheckBox *> m_multipleChoice;

    QPushButton *m_startExam = nullptr;
    QPushButton *m_startTraining = nullptr;
    QPushButton *m_back = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_finish = nullptr;
};

} // namespace hrt