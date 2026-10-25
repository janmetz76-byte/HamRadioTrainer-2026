// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "exam/ExamSession.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace hrt {

class Trainer;

/// One simulated examination on the phone.
///
/// The same paper and the same grading as on the desktop: 25 questions per
/// chosen Prüfungsteil, one clock for the whole paper, 19 points per part to
/// pass, unanswered questions count as wrong. Only the parts of the chosen
/// case are examined, so an ascent from N to E sits the E sheet alone.
class ExamRound : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool finished READ finished NOTIFY changed)
    Q_PROPERTY(int index READ index NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QVariantMap question READ question NOTIFY changed)
    Q_PROPERTY(int selection READ selection NOTIFY changed)
    Q_PROPERTY(QString partLetter READ partLetter NOTIFY changed)
    Q_PROPERTY(QString partName READ partName NOTIFY changed)
    Q_PROPERTY(int answeredCount READ answeredCount NOTIFY changed)
    Q_PROPERTY(QVariantList answered READ answered NOTIFY changed)
    Q_PROPERTY(int secondsLeft READ secondsLeft NOTIFY clockChanged)
    Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
    Q_PROPERTY(QVariantMap plan READ plan NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    explicit ExamRound(Trainer *trainer);

    bool running() const { return m_session.has_value(); }
    bool finished() const { return !m_result.isEmpty(); }
    int index() const { return m_index; }
    int count() const { return m_session ? m_session->count() : 0; }
    QVariantMap question() const { return m_question; }
    int selection() const;
    QString partLetter() const;
    QString partName() const;
    int answeredCount() const { return m_session ? m_session->answeredCount() : 0; }
    QVariantList answered() const;
    int secondsLeft() const;
    QVariantMap result() const { return m_result; }
    /// What the next paper will contain: parts, questions and minutes.
    QVariantMap plan() const;
    QString message() const { return m_message; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void select(int position);
    Q_INVOKABLE void go(int index);
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    /// Grades the paper and puts the outcome into the learning progress.
    Q_INVOKABLE void finish();
    /// Gives the paper up without grading it.
    Q_INVOKABLE void abort();
    /// Leaves the result page.
    Q_INVOKABLE void dismissResult();

    void reset();

signals:
    void changed();
    void clockChanged();

private:
    PaperBuilder::Options options() const;
    void showCurrent();
    void tick();

    Trainer *m_trainer;
    std::optional<ExamSession> m_session;
    int m_index = 0;
    QVariantMap m_question;
    QVariantMap m_result;
    QString m_message;
    QElapsedTimer m_clock;
    QTimer m_timer;
};

} // namespace hrt
