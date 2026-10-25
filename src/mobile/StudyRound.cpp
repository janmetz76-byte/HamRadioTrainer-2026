// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "StudyRound.h"

#include "Trainer.h"

#include "exam/PaperBuilder.h"
#include "learning/LearningStatistics.h"

#include <QRandomGenerator>
#include <QSettings>

namespace hrt {

StudyRound::StudyRound(Trainer *trainer)
    : QObject(trainer)
    , m_trainer(trainer)
{
    // Derselbe Schluessel wie die Lernansicht am Desktop.
    QSettings settings;
    m_showSolution = settings.value(QStringLiteral("learnview/showSolution"), true).toBool();
}

QString StudyRound::partLetter() const
{
    if (!m_session || m_index >= m_session->count())
        return {};
    return m_session->partLetterOf(m_session->plan().at(m_index));
}

void StudyRound::setRoundSize(int size)
{
    size = qBound(1, size, 200);
    if (size == m_roundSize)
        return;
    m_roundSize = size;
    emit optionsChanged();
}

void StudyRound::setOnlyDue(bool on)
{
    if (on == m_onlyDue)
        return;
    m_onlyDue = on;
    emit optionsChanged();
}

void StudyRound::setOnlyShaky(bool on)
{
    if (on == m_onlyShaky)
        return;
    m_onlyShaky = on;
    emit optionsChanged();
}

void StudyRound::setShowSolution(bool on)
{
    if (on == m_showSolution)
        return;
    m_showSolution = on;
    QSettings settings;
    settings.setValue(QStringLiteral("learnview/showSolution"), on);
    emit optionsChanged();
}

void StudyRound::setPartFilter(const QString &letter)
{
    if (letter == m_partFilter)
        return;
    m_partFilter = letter;
    emit optionsChanged();
}

QVariantList StudyRound::partChoices() const
{
    QVariantList choices;
    const Catalog &catalog = m_trainer->catalog();
    const QSet<int> selected = effectiveParts(catalog, m_trainer->selectedParts());
    for (int i = 0; i < catalog.parts.size(); ++i) {
        if (!selected.contains(i))
            continue;
        choices.append(QVariantMap{
            {QStringLiteral("letter"), examPartLetter(catalog.parts.at(i))},
            {QStringLiteral("title"), catalog.parts.at(i).title},
        });
    }
    return choices;
}

void StudyRound::start()
{
    const Catalog &catalog = m_trainer->catalog();
    StudyOptions options;
    options.maxQuestions = m_roundSize;
    options.onlyDue = m_onlyDue;
    options.onlyShaky = m_onlyShaky;
    if (!m_partFilter.isEmpty()) {
        options.parts = {m_partFilter};
    } else {
        // "alle" heisst: alle gewaehlten Teile, nicht der ganze Katalog.
        const QSet<int> selected = effectiveParts(catalog, m_trainer->selectedParts());
        for (int i = 0; i < catalog.parts.size(); ++i) {
            if (selected.contains(i))
                options.parts.append(examPartLetter(catalog.parts.at(i)));
        }
    }

    // Ein Zufallswert je Runde fuer Fragenreihenfolge und Antworten; nie 0,
    // das hiesse "feste Reihenfolge".
    const quint32 seed = QRandomGenerator::global()->bounded(1u, 0xffffffffu);
    options.shuffleSeed = seed;
    StudySession session(catalog, &m_trainer->scheduler(), options);
    if (session.count() == 0) {
        m_message = tr("Zu dieser Auswahl gibt es nichts zu üben.");
        emit changed();
        return;
    }
    m_session.emplace(std::move(session));
    m_fresh = m_session->freshCount();
    m_index = 0;
    m_correct = 0;
    m_finished = false;
    m_message.clear();
    m_seed = seed;
    showCurrent();
}

void StudyRound::showCurrent()
{
    m_selection = -1;
    m_revealed = false;
    m_correctPosition = -1;
    m_feedback.clear();
    m_question.clear();
    m_order.clear();

    if (!m_session || m_index >= m_session->count()) {
        m_finished = m_session.has_value();
        emit changed();
        return;
    }

    const QString id = m_session->plan().at(m_index);
    const QHash<QString, const Question *> byId = questionIndex(m_trainer->catalog());
    const Question *question = byId.value(id, nullptr);
    if (!question) {
        ++m_index;
        showCurrent();
        return;
    }
    m_order = PaperBuilder::shuffledAnswerOrder(*question, m_seed, id);
    for (int position = 0; position < m_order.size(); ++position) {
        const int answer = m_order.at(position);
        if (answer >= 0 && answer < question->answers.size()
            && question->answers.at(answer).correct)
            m_correctPosition = position;
    }
    m_question = m_trainer->presentQuestion(*question, m_order);
    emit changed();
}

void StudyRound::select(int position)
{
    // Vor der Einschaetzung laesst sich die Wahl noch aendern, danach nicht.
    if (!running() || m_revealed || m_question.isEmpty())
        return;
    m_selection = position;
    emit changed();
}

void StudyRound::rate(int rating)
{
    if (!running() || m_revealed || m_selection < 0)
        return;

    const QString id = m_question.value(QStringLiteral("id")).toString();
    const bool correct = m_selection == m_correctPosition;
    if (correct)
        ++m_correct;
    m_session->record(id, correct, static_cast<Confidence>(rating));
    m_trainer->saveProgress();

    if (!m_showSolution) {
        ++m_index;
        showCurrent();
        return;
    }

    // Erst jetzt die Aufloesung, mit den angezeigten Buchstaben.
    const QString chosen = QString(QChar(QLatin1Char('A').unicode() + m_selection));
    const QString right = QString(QChar(QLatin1Char('A').unicode() + m_correctPosition));
    m_feedback = correct ? tr("Antwort %1 ist richtig.").arg(chosen)
                         : tr("Antwort %1 ist leider falsch. Richtig ist %2.").arg(chosen, right);
    m_revealed = true;
    emit changed();
}

void StudyRound::next()
{
    if (!running())
        return;
    ++m_index;
    showCurrent();
}

void StudyRound::stop()
{
    if (!m_session)
        return;
    // Bisher Beantwortetes ist schon nach jeder Antwort gespeichert.
    reset();
}

void StudyRound::reset()
{
    m_session.reset();
    m_index = 0;
    m_correct = 0;
    m_fresh = 0;
    m_finished = false;
    m_message.clear();
    m_question.clear();
    m_order.clear();
    m_selection = -1;
    m_revealed = false;
    m_feedback.clear();
    emit changed();
    emit optionsChanged();
}

} // namespace hrt
