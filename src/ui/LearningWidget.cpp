// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "LearningWidget.h"

#include "AnswerView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace hrt {
namespace {

/// Box 0 is where wrong answers land, the top box is what is really secure.
QString boxLabel(int box)
{
    if (box <= 0)
        return QObject::tr("wackelig");
    if (box >= Scheduler::MaxBox)
        return QObject::tr("gefestigt");
    return QObject::tr("Ordner %1").arg(box);
}

} // namespace

LearningWidget::LearningWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    m_summary = new QLabel(this);
    QFont summaryFont = m_summary->font();
    summaryFont.setBold(true);
    m_summary->setFont(summaryFont);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    m_boxes = new QTableWidget(1, Scheduler::MaxBox + 1, this);
    m_boxes->setObjectName(QStringLiteral("leitnerOrdner"));
    m_boxes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_boxes->setSelectionMode(QAbstractItemView::NoSelection);
    m_boxes->verticalHeader()->setVisible(false);
    m_boxes->horizontalHeader()->setVisible(false);
    // Without stretching, the columns keep their default width and the labels
    // are cut off.
    m_boxes->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_boxes->setWordWrap(false);
    m_boxes->verticalHeader()->setDefaultSectionSize(64);
    m_boxes->setFixedHeight(72);
    for (int box = 0; box <= Scheduler::MaxBox; ++box)
        m_boxes->setItem(0, box, new QTableWidgetItem(boxLabel(box)));
    layout->addWidget(m_boxes);

    m_parts = new QTableWidget(0, 6, this);
    m_parts->setObjectName(QStringLiteral("fortschrittJePruefungsteil"));
    m_parts->setHorizontalHeaderLabels({tr("Prüfungsteil"), tr("Fragen"), tr("beantwortet"),
                                         tr("gefestigt"), tr("wackelig"), tr("Trefferquote")});
    m_parts->verticalHeader()->setVisible(false);
    m_parts->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_parts->setSelectionMode(QAbstractItemView::NoSelection);
    m_parts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_parts->setMaximumHeight(200);
    layout->addWidget(m_parts);

    auto *options = new QGroupBox(tr("Übungsrunde"), this);
    auto *optionsLayout = new QHBoxLayout(options);
    optionsLayout->addWidget(new QLabel(tr("Fragen:"), options));
    m_roundSize = new QSpinBox(options);
    m_roundSize->setRange(1, 200);
    m_roundSize->setValue(20);
    optionsLayout->addWidget(m_roundSize);
    m_onlyDue = new QCheckBox(tr("nur fällige"), options);
    m_onlyDue->setToolTip(tr("Nur Wiederholungen, deren Fälligkeit erreicht ist."));
    optionsLayout->addWidget(m_onlyDue);
    m_onlyShaky = new QCheckBox(tr("nur wackelige"), options);
    m_onlyShaky->setToolTip(tr("Nur Fragen, die zuletzt falsch beantwortet wurden."));
    optionsLayout->addWidget(m_onlyShaky);
    optionsLayout->addWidget(new QLabel(tr("Prüfungsteil:"), options));
    m_partFilter = new QComboBox(options);
    optionsLayout->addWidget(m_partFilter);
    optionsLayout->addStretch(1);
    m_start = new QPushButton(tr("Runde starten"), options);
    optionsLayout->addWidget(m_start);
    layout->addWidget(options);

    m_progress = new QLabel(this);
    layout->addWidget(m_progress);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    m_answers = new AnswerView(scroll);
    scroll->setWidget(m_answers);
    layout->addWidget(scroll, 1);

    m_explanation = new QLabel(this);
    m_explanation->setWordWrap(true);
    layout->addWidget(m_explanation);

    // The self report. Without it a correctly answered but guessed question
    // would be treated like a question that was known.
    auto *confidenceRow = new QHBoxLayout;
    confidenceRow->addWidget(new QLabel(tr("Wie sicher war die Antwort?"), this));
    m_confident = new QPushButton(tr("Sicher"), this);
    m_unsure = new QPushButton(tr("Unsicher"), this);
    m_guessing = new QPushButton(tr("Geraten"), this);
    for (QPushButton *button : {m_confident, m_unsure, m_guessing})
        confidenceRow->addWidget(button);
    confidenceRow->addStretch(1);
    m_nextQuestion = new QPushButton(tr("Nächste Frage"), this);
    confidenceRow->addWidget(m_nextQuestion);
    layout->addLayout(confidenceRow);

    connect(m_start, &QPushButton::clicked, this, &LearningWidget::startRound);
    connect(m_answers, &AnswerView::selected, this, &LearningWidget::onAnswerSelected);
    connect(m_confident, &QPushButton::clicked, this,
            [this] { rateConfidence(int(Confidence::Certain)); });
    connect(m_unsure, &QPushButton::clicked, this,
            [this] { rateConfidence(int(Confidence::Unsure)); });
    connect(m_guessing, &QPushButton::clicked, this,
            [this] { rateConfidence(int(Confidence::Guessing)); });
    connect(m_nextQuestion, &QPushButton::clicked, this, [this] {
        ++m_index;
        showCurrent();
    });

    refresh();
}

void LearningWidget::setCatalog(const Catalog *catalog, const QString &catalogPath)
{
    m_catalog = catalog;
    m_catalogDir = QFileInfo(catalogPath).absolutePath();
    m_byId = catalog ? questionIndex(*catalog) : QHash<QString, const Question *>();
    m_session.reset();
    m_index = 0;
    m_answers->setQuestion(nullptr, {}, QDir());
    m_explanation->clear();
    m_progress->clear();
    refreshPartFilter();
    refresh();
}

void LearningWidget::setProgress(const QVector<QuestionState> &states)
{
    m_scheduler = Scheduler(states);
    refresh();
}

void LearningWidget::clearProgress()
{
    m_scheduler.reset();
    refresh();
}

void LearningWidget::refreshPartFilter()
{
    const QString previous = m_partFilter->currentData().toString();
    m_partFilter->clear();
    m_partFilter->addItem(tr("alle"), QString());
    if (m_catalog) {
        for (const Part &part : m_catalog->parts) {
            const QString letter = examPartLetter(part);
            if (letter.isEmpty())
                continue;
            m_partFilter->addItem(QStringLiteral("%1 — %2").arg(letter, part.title), letter);
        }
    }
    const int index = m_partFilter->findData(previous);
    if (index >= 0)
        m_partFilter->setCurrentIndex(index);
}

void LearningWidget::refresh()
{
    m_parts->setRowCount(0);
    if (!m_catalog) {
        m_summary->setText(tr("Kein Katalog geladen"));
        return;
    }

    const LearningStatistics statistics = computeStatistics(*m_catalog, m_scheduler);

    // No percent sign in the format string: QString::arg only unescapes "%%"
    // on the first call, so it would be shown literally.
    m_summary->setText(tr("%1 von %2 Fragen beantwortet · %3 Prozent bearbeitet · "
                          "Trefferquote %4 Prozent · %5 fällig · %6 neu · %7 wackelig · "
                          "%8 gefestigt")
                           .arg(statistics.answered)
                           .arg(statistics.questions)
                           .arg(statistics.coverage(), 0, 'f', 1)
                           .arg(statistics.accuracy(), 0, 'f', 1)
                           .arg(statistics.due)
                           .arg(statistics.fresh)
                           .arg(statistics.shaky)
                           .arg(statistics.mastered));

    for (int box = 0; box <= Scheduler::MaxBox && box < m_boxes->columnCount(); ++box) {
        const int count = statistics.byBox.value(box);
        auto *item = m_boxes->item(0, box);
        if (!item)
            continue;
        item->setText(QStringLiteral("%1\n%2").arg(boxLabel(box)).arg(count));
        item->setTextAlignment(Qt::AlignCenter);
        // A bar chart made of text: the empty column would not read as a
        // proportion at a glance.
        // Darker the more questions sit in the box, so the shape of the
        // distribution can be read at a glance.
        const int share = statistics.answered > 0 ? count * 100 / statistics.answered : 0;
        const QColor base(0x6f, 0xa3, 0x4f);
        item->setBackground(count == 0 ? QColor(0xf2, 0xf2, 0xf2)
                                       : base.lighter(150 - qBound(0, share, 100) * 3 / 5));
        QFont font = item->font();
        font.setBold(count > 0);
        item->setFont(font);
    }

    m_parts->setRowCount(statistics.parts.size());
    for (int i = 0; i < statistics.parts.size(); ++i) {
        const PartProgress &part = statistics.parts.at(i);
        const int row = i;
        m_parts->setItem(row, 0, new QTableWidgetItem(QStringLiteral("%1 — %2").arg(
                                                             part.letter, part.title)));
        m_parts->setItem(row, 1, new QTableWidgetItem(QString::number(part.questions)));
        m_parts->setItem(row, 2, new QTableWidgetItem(QString::number(part.answered)));
        m_parts->setItem(row, 3, new QTableWidgetItem(QString::number(part.mastered)));
        m_parts->setItem(row, 4, new QTableWidgetItem(QString::number(part.shaky)));
        m_parts->setItem(row, 5, new QTableWidgetItem(
                                     QStringLiteral("%1 %").arg(part.accuracy(), 0, 'f', 1)));
    }
}

StudyOptions LearningWidget::currentOptions() const
{
    StudyOptions options;
    options.maxQuestions = m_roundSize->value();
    options.onlyDue = m_onlyDue->isChecked();
    options.onlyShaky = m_onlyShaky->isChecked();
    const QString part = m_partFilter->currentData().toString();
    if (!part.isEmpty())
        options.parts = {part};
    return options;
}

void LearningWidget::startRound()
{
    if (!m_catalog)
        return;

    StudySession session(*m_catalog, &m_scheduler, currentOptions());
    if (session.count() == 0) {
        m_progress->setText(tr("Zu dieser Auswahl gibt es nichts zu üben."));
        return;
    }

    m_session.emplace(std::move(session));
    m_index = 0;
    m_correct = 0;
    m_seed = QRandomGenerator::global()->generate();
    m_explanation->clear();
    showCurrent();
}

void LearningWidget::showCurrent()
{
    const bool running = m_session.has_value() && m_index < m_session->count();

    m_start->setEnabled(m_catalog && !running);
    m_roundSize->setEnabled(!running);
    m_onlyDue->setEnabled(!running);
    m_onlyShaky->setEnabled(!running);
    m_partFilter->setEnabled(!running);
    for (QPushButton *button : {m_confident, m_unsure, m_guessing, m_nextQuestion})
        button->setEnabled(false);

    if (!running) {
        if (m_session && m_index >= m_session->count() && m_session->count() > 0) {
            showSummary();
            return;
        }
        m_answers->setQuestion(nullptr, {}, QDir());
        m_progress->clear();
        return;
    }

    const QString id = m_session->plan().at(m_index);
    const Question *question = m_byId.value(id, nullptr);
    if (!question) {
        ++m_index;
        showCurrent();
        return;
    }
    m_pendingQuestionId = id;

    m_progress->setText(tr("Frage %1 von %2  ·  Prüfungsteil %3")
                            .arg(m_index + 1)
                            .arg(m_session->count())
                            .arg(m_session->partLetterOf(id)));

    m_answers->setQuestion(question, PaperBuilder::shuffledAnswerOrder(*question, m_seed, id),
                           QDir(m_catalogDir));
}

void LearningWidget::onAnswerSelected(int position)
{
    if (!m_session || position < 0)
        return;

    const Question *question = m_byId.value(m_pendingQuestionId, nullptr);
    if (!question)
        return;

    const QVector<int> &order = PaperBuilder::shuffledAnswerOrder(*question, m_seed,
                                                                   m_pendingQuestionId);
    int correctPosition = -1;
    for (int i = 0; i < order.size(); ++i) {
        const int answerIndex = order.at(i);
        if (answerIndex >= 0 && answerIndex < question->answers.size()
            && question->answers.at(answerIndex).correct)
            correctPosition = i;
    }
    m_pendingCorrectPosition = correctPosition;
    const bool correct = position == correctPosition;
    if (correct)
        ++m_correct;

    m_answers->reveal(correctPosition, position);
    m_explanation->setText(correct ? tr("Richtig.") : tr("Falsch."));
    if (!question->explanation.isEmpty())
        m_explanation->setText(m_explanation->text() + QLatin1Char('\n')
                               + question->explanation);

    // The self report is asked only now: the learner has to have judged their
    // own answer, not the correct one.
    for (QPushButton *button : {m_confident, m_unsure, m_guessing})
        button->setEnabled(true);
}

void LearningWidget::rateConfidence(int confidence)
{
    if (!m_session || m_pendingQuestionId.isEmpty())
        return;

    const Question *question = m_byId.value(m_pendingQuestionId, nullptr);
    const bool correct = question && m_pendingCorrectPosition >= 0
                         && m_answers->selection() == m_pendingCorrectPosition;

    m_session->record(m_pendingQuestionId, correct, static_cast<Confidence>(confidence));
    m_pendingQuestionId.clear();

    emit progressChanged(m_scheduler);
    refresh();

    ++m_index;
    showCurrent();
}

void LearningWidget::showSummary()
{
    const int total = m_session ? m_session->count() : 0;
    m_answers->setQuestion(nullptr, {}, QDir());
    m_explanation->clear();
    m_progress->setText(tr("Runde beendet: %1 von %2 Fragen richtig (%3 %)  ·  %4 neu dabei")
                            .arg(m_correct)
                            .arg(total)
                            .arg(total > 0 ? 100 * m_correct / total : 0)
                            .arg(m_session ? m_session->freshCount() : 0));
    m_session.reset();
    m_index = 0;
    refresh();
}

} // namespace hrt