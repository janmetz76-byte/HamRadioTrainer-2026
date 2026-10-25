// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ExamRound.h"

#include "Trainer.h"

#include "learning/LearningStatistics.h"

#include <QRandomGenerator>

namespace hrt {
namespace {

/// "V (Kenntnisse von Vorschriften)" becomes "Kenntnisse von Vorschriften".
QString shortPartName(QString title, const QString &letter)
{
    const int authority = title.indexOf(QStringLiteral(" – "));
    if (authority > 0)
        title = title.left(authority);
    if (title.startsWith(letter))
        title = title.mid(letter.size()).trimmed();
    if (title.startsWith(QLatin1Char('(')) && title.endsWith(QLatin1Char(')')))
        title = title.mid(1, title.size() - 2).trimmed();
    return title;
}

} // namespace

ExamRound::ExamRound(Trainer *trainer)
    : QObject(trainer)
    , m_trainer(trainer)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] { tick(); });
}

PaperBuilder::Options ExamRound::options() const
{
    // Wie am Desktop: nicht gewaehlte Teile bekommen keine Fragen und
    // entfallen ganz, statt mit einer leeren Seite durchzufallen.
    const Catalog &catalog = m_trainer->catalog();
    PaperBuilder::Options options = PaperBuilder::examOptions(catalog);
    const QSet<int> selected = effectiveParts(catalog, m_trainer->selectedParts());
    for (int i = 0; i < options.questionsPerPart.size(); ++i) {
        if (!selected.contains(i))
            options.questionsPerPart[i] = 0;
    }
    return options;
}

QVariantMap ExamRound::plan() const
{
    const Catalog &catalog = m_trainer->catalog();
    const PaperBuilder::Options planned = options();
    QVariantList parts;
    int questions = 0;
    int minutes = 0;
    for (int i = 0; i < planned.questionsPerPart.size() && i < catalog.parts.size(); ++i) {
        const int count = planned.questionsPerPart.at(i);
        if (count <= 0)
            continue;
        const Part &part = catalog.parts.at(i);
        const int partMinutes = part.selection.minutes.value_or(0);
        questions += count;
        minutes += partMinutes;
        parts.append(QVariantMap{
            {QStringLiteral("letter"), examPartLetter(part)},
            {QStringLiteral("name"), shortPartName(part.title, examPartLetter(part))},
            {QStringLiteral("questions"), count},
            {QStringLiteral("minutes"), partMinutes},
            {QStringLiteral("passScore"), part.selection.passScore.value_or(0)},
        });
    }
    return QVariantMap{
        {QStringLiteral("parts"), parts},
        {QStringLiteral("questions"), questions},
        {QStringLiteral("minutes"), minutes},
    };
}

void ExamRound::start()
{
    const Catalog &catalog = m_trainer->catalog();
    const PaperBuilder::Options planned = options();
    if (planned.questionsPerPart.size() != catalog.parts.size()) {
        m_message = tr("Der Katalog deklariert keinen Prüfungsumfang.");
        emit changed();
        return;
    }
    const quint32 seed = QRandomGenerator::global()->generate();
    const ExamPaper paper = PaperBuilder::build(catalog, m_trainer->scheduler(), planned, seed);
    if (paper.questions.isEmpty()) {
        m_message = tr("Für diese Auswahl lässt sich kein Prüfungsbogen bilden.");
        emit changed();
        return;
    }
    m_session.emplace(catalog, paper);
    m_index = 0;
    m_result.clear();
    m_message.clear();
    m_clock.start();
    m_timer.start();
    showCurrent();
    emit clockChanged();
}

void ExamRound::showCurrent()
{
    m_question.clear();
    if (m_session && m_index >= 0 && m_index < m_session->count()) {
        if (const Question *question = m_session->questionAt(m_index))
            m_question = m_trainer->presentQuestion(*question, m_session->answerOrderAt(m_index));
    }
    emit changed();
}

int ExamRound::selection() const
{
    return m_session ? m_session->selectedPosition(m_index) : -1;
}

QString ExamRound::partLetter() const
{
    return m_session ? m_session->partLetterAt(m_index) : QString();
}

QString ExamRound::partName() const
{
    return m_session ? shortPartName(m_session->partTitleAt(m_index), partLetter()) : QString();
}

QVariantList ExamRound::answered() const
{
    QVariantList flags;
    if (!m_session)
        return flags;
    for (int i = 0; i < m_session->count(); ++i)
        flags.append(m_session->isAnswered(i));
    return flags;
}

int ExamRound::secondsLeft() const
{
    if (!m_session)
        return 0;
    const qint64 total = qint64(m_session->paper().totalMinutes) * 60;
    return int(qMax<qint64>(0, total - m_clock.elapsed() / 1000));
}

void ExamRound::tick()
{
    emit clockChanged();
    // Wie in der echten Pruefung: ist die Zeit um, wird abgegeben.
    if (m_session && secondsLeft() <= 0)
        finish();
}

void ExamRound::select(int position)
{
    if (!m_session)
        return;
    m_session->answer(m_index, position);
    emit changed();
}

void ExamRound::go(int index)
{
    if (!m_session || index < 0 || index >= m_session->count())
        return;
    m_index = index;
    showCurrent();
}

void ExamRound::next()
{
    go(m_index + 1);
}

void ExamRound::previous()
{
    go(m_index - 1);
}

void ExamRound::finish()
{
    if (!m_session)
        return;
    m_timer.stop();

    const ExamResult result = m_session->evaluate();

    // Wie am Desktop: jede Frage des Bogens geht in den Lernstand ein, ohne
    // Selbsteinschaetzung - eine Pruefung fragt nicht, wie sicher man war.
    for (int i = 0; i < m_session->count(); ++i) {
        AnswerOutcome outcome;
        outcome.questionId = m_session->questionAt(i) ? m_session->questionAt(i)->id
                                                      : m_session->paper().questions.at(i).questionId;
        const int selected = m_session->selectedPosition(i);
        const int correct = m_session->correctPositionAt(i);
        outcome.correct = selected >= 0 && correct >= 0 && selected == correct;
        outcome.confidence = Confidence::Unknown;
        m_trainer->scheduler().record(outcome);
    }
    m_trainer->saveProgress();

    QVariantList parts;
    for (const PartResult &part : result.parts) {
        parts.append(QVariantMap{
            {QStringLiteral("letter"), part.letter},
            {QStringLiteral("name"), shortPartName(part.title, part.letter)},
            {QStringLiteral("achieved"), part.achievedPoints},
            {QStringLiteral("max"), part.maxPoints},
            {QStringLiteral("passScore"), part.passScore},
            {QStringLiteral("passed"), part.passed()},
            {QStringLiteral("wrong"), part.wrongAnswers},
            {QStringLiteral("unanswered"), part.unanswered},
        });
    }
    m_result = QVariantMap{
        {QStringLiteral("passed"), result.passed},
        {QStringLiteral("achieved"), result.achievedPoints},
        {QStringLiteral("max"), result.maxPoints},
        {QStringLiteral("failedParts"), result.failedParts().join(QStringLiteral(", "))},
        {QStringLiteral("oralRetestPart"), result.oralRetestPart()},
        {QStringLiteral("parts"), parts},
    };
    m_session.reset();
    m_index = 0;
    m_question.clear();
    emit changed();
}

void ExamRound::abort()
{
    m_timer.stop();
    m_session.reset();
    m_index = 0;
    m_question.clear();
    emit changed();
}

void ExamRound::dismissResult()
{
    m_result.clear();
    emit changed();
}

void ExamRound::reset()
{
    abort();
    m_result.clear();
    m_message.clear();
    emit changed();
}

} // namespace hrt
