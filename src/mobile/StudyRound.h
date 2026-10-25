// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "learning/StudySession.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace hrt {

class Trainer;

/// One practice round on the phone.
///
/// The same order as on the desktop and in the 2015 trainer: pick an answer,
/// say how sure you were, and only then learn whether it was right. The
/// solution can be switched off; the round then moves straight on and the
/// result is told at the end.
class StudyRound : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool finished READ finished NOTIFY changed)
    Q_PROPERTY(int index READ index NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QString partLetter READ partLetter NOTIFY changed)
    Q_PROPERTY(QVariantMap question READ question NOTIFY changed)
    Q_PROPERTY(int selection READ selection NOTIFY changed)
    Q_PROPERTY(bool revealed READ revealed NOTIFY changed)
    Q_PROPERTY(int correctPosition READ correctPosition NOTIFY changed)
    Q_PROPERTY(QString feedback READ feedback NOTIFY changed)
    Q_PROPERTY(int correctCount READ correctCount NOTIFY changed)
    Q_PROPERTY(int freshCount READ freshCount NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

    Q_PROPERTY(int roundSize READ roundSize WRITE setRoundSize NOTIFY optionsChanged)
    Q_PROPERTY(bool onlyDue READ onlyDue WRITE setOnlyDue NOTIFY optionsChanged)
    Q_PROPERTY(bool onlyShaky READ onlyShaky WRITE setOnlyShaky NOTIFY optionsChanged)
    Q_PROPERTY(bool showSolution READ showSolution WRITE setShowSolution NOTIFY optionsChanged)
    Q_PROPERTY(QString partFilter READ partFilter WRITE setPartFilter NOTIFY optionsChanged)
    Q_PROPERTY(QVariantList partChoices READ partChoices NOTIFY optionsChanged)

public:
    /// The self report, as the scheduler knows it.
    enum Rating { Guessed = 0, Unsure = 1, Sure = 3 };
    Q_ENUM(Rating)

    explicit StudyRound(Trainer *trainer);

    bool running() const { return m_session.has_value() && !m_finished; }
    bool finished() const { return m_finished; }
    int index() const { return m_index; }
    int count() const { return m_session ? m_session->count() : 0; }
    QString partLetter() const;
    QVariantMap question() const { return m_question; }
    int selection() const { return m_selection; }
    bool revealed() const { return m_revealed; }
    int correctPosition() const { return m_revealed ? m_correctPosition : -1; }
    QString feedback() const { return m_feedback; }
    int correctCount() const { return m_correct; }
    int freshCount() const { return m_fresh; }
    QString message() const { return m_message; }

    int roundSize() const { return m_roundSize; }
    void setRoundSize(int size);
    bool onlyDue() const { return m_onlyDue; }
    void setOnlyDue(bool on);
    bool onlyShaky() const { return m_onlyShaky; }
    void setOnlyShaky(bool on);
    bool showSolution() const { return m_showSolution; }
    void setShowSolution(bool on);
    QString partFilter() const { return m_partFilter; }
    void setPartFilter(const QString &letter);
    /// The Prüfungsteile the learner prepares for: letter and title.
    QVariantList partChoices() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void select(int position);
    Q_INVOKABLE void rate(int rating);
    Q_INVOKABLE void next();
    /// Ends the round early. What was answered is kept.
    Q_INVOKABLE void stop();

    /// Forgets the running round, e.g. after the catalog changed.
    void reset();

signals:
    void changed();
    void optionsChanged();

private:
    void showCurrent();

    Trainer *m_trainer;
    std::optional<StudySession> m_session;
    int m_index = 0;
    quint32 m_seed = 0;
    QVariantMap m_question;
    QVector<int> m_order;
    int m_selection = -1;
    bool m_revealed = false;
    int m_correctPosition = -1;
    QString m_feedback;
    int m_correct = 0;
    int m_fresh = 0;
    bool m_finished = false;
    QString m_message;

    int m_roundSize = 20;
    bool m_onlyDue = false;
    bool m_onlyShaky = false;
    bool m_showSolution = true;
    QString m_partFilter;
};

} // namespace hrt
