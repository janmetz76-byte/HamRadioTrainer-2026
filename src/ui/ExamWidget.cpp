// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ExamWidget.h"

#include "ImageLoader.h"
#include "exam/PaperBuilder.h"
#include "learning/Scheduler.h"

#include <QAbstractButton>
#include <QLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace hrt {
namespace {

/// Longest edge of a question or answer image in the exam view.
constexpr int kMaxImageWidth = 640;

/// LaTeX fragments are shown as they are stored. The source keeps 219 of them
/// in class A; rendering them is a separate step and must not be guessed.
QString plainTextOf(const Question *question)
{
    QString text = question->text;
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return text.simplified();
}

/// Builds one answer row: the selection control plus the answer text in a
/// label that wraps. QRadioButton and QCheckBox do not wrap their own text, so
/// a long answer would be cut off at the window edge.
QWidget *makeAnswerRow(QWidget *parent, QAbstractButton *control, const QString &text)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    control->setText(QString());
    control->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    layout->addWidget(control, 0, Qt::AlignTop);

    auto *label = new QLabel(text, row);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(label, 1);

    row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    return row;
}

QString formatMinutesLeft(qint64 seconds)
{
    if (seconds < 0)
        seconds = 0;
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    return QStringLiteral("%1:%2").arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'));
}

} // namespace

ExamWidget::ExamWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    updateNavigation();
}

void ExamWidget::buildUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *startRow = new QHBoxLayout;
    m_startExam = new QPushButton(tr("Prüfung starten"), this);
    m_startExam->setToolTip(tr("25 Fragen je Prüfungsteil, bestanden ab 19 Punkten, "
                               "45 Minuten je Teil, 60 Minuten für Klasse A."));
    connect(m_startExam, &QPushButton::clicked, this, &ExamWidget::startExam);
    startRow->addWidget(m_startExam);

    startRow->addWidget(new QLabel(tr("Training:"), this));
    auto *trainingCount = new QSpinBox(this);
    trainingCount->setRange(1, 200);
    trainingCount->setValue(25);
    startRow->addWidget(trainingCount);
    m_startTraining = new QPushButton(tr("Üben"), this);
    connect(m_startTraining, &QPushButton::clicked, this,
            [this, trainingCount] { startTraining(trainingCount->value()); });
    startRow->addWidget(m_startTraining);

    startRow->addStretch(1);
    m_timerLabel = new QLabel(this);
    QFont timerFont = m_timerLabel->font();
    timerFont.setBold(true);
    timerFont.setPointSizeF(timerFont.pointSizeF() * 1.4);
    m_timerLabel->setFont(timerFont);
    m_timerLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    // Reserve room for the longest possible text, otherwise the countdown is
    // clipped to a few pixels while it counts.
    m_timerLabel->setMinimumWidth(
        QFontMetrics(timerFont).horizontalAdvance(tr("Restzeit 00:00")) + 8);
    startRow->addWidget(m_timerLabel);
    layout->addLayout(startRow);

    m_header = new QLabel(this);
    QFont headerFont = m_header->font();
    headerFont.setBold(true);
    m_header->setFont(headerFont);
    layout->addWidget(m_header);

    m_question = new QLabel(this);
    m_question->setWordWrap(true);
    m_question->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_question);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    m_answerArea = new QWidget(scroll);
    m_answerLayout = new QVBoxLayout(m_answerArea);
    // Wrapping labels only report the right height for the width they really
    // get if the layout asks for it, which also lets the scroll area shrink the
    // rows to their content instead of stretching them over the window.
    m_answerLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    scroll->setWidget(m_answerArea);
    layout->addWidget(scroll, 1);

    m_progress = new QLabel(this);
    layout->addWidget(m_progress);

    auto *navRow = new QHBoxLayout;
    m_back = new QPushButton(tr("Zurück"), this);
    connect(m_back, &QPushButton::clicked, this, &ExamWidget::goBack);
    navRow->addWidget(m_back);

    m_next = new QPushButton(tr("Nächste unbeantwortete Frage"), this);
    connect(m_next, &QPushButton::clicked, this, &ExamWidget::goToNextUnanswered);
    navRow->addWidget(m_next);

    navRow->addStretch(1);
    m_finish = new QPushButton(tr("Auswerten"), this);
    connect(m_finish, &QPushButton::clicked, this, &ExamWidget::finishExam);
    navRow->addWidget(m_finish);
    layout->addLayout(navRow);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &ExamWidget::tick);
}

void ExamWidget::setCatalog(const Catalog *catalog, const QString &catalogPath)
{
    m_catalog = catalog;
    m_catalogDir = QFileInfo(catalogPath).absolutePath();
    m_session.reset();
    m_current = 0;
    m_timer->stop();
    clearQuestionArea();
    m_header->clear();
    m_timerLabel->clear();
    updateNavigation();
}

void ExamWidget::clearQuestionArea()
{
    delete m_singleChoice;
    m_singleChoice = nullptr;
    qDeleteAll(m_multipleChoice);
    m_multipleChoice.clear();

    // Remove the answer rows and the image labels, keep the layout itself.
    while (QLayoutItem *item = m_answerLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
}

/// Shows the freshly built answer rows.
///
/// Widgets that are added to the layout of an already visible parent only
/// become visible when the layout is activated. Without this the answers would
/// stay invisible until some other event forces a layout pass.
void ExamWidget::activateAnswerArea()
{
    if (!m_answerLayout)
        return;
    m_answerLayout->activate();
    for (int i = 0; i < m_answerLayout->count(); ++i) {
        if (QWidget *widget = m_answerLayout->itemAt(i)->widget())
            widget->show();
    }
}

QPixmap ExamWidget::renderImage(const QString &relativePath) const
{
    return images::load(QDir(m_catalogDir), relativePath);
}

void ExamWidget::setProgress(const QVector<QuestionState> &states)
{
    m_scheduler = Scheduler(states);
}

void ExamWidget::clearProgress()
{
    m_scheduler.reset();
}

bool ExamWidget::startExam()
{
    if (!m_catalog)
        return false;
    const PaperBuilder::Options options = PaperBuilder::examOptions(*m_catalog);
    if (options.questionsPerPart.size() != m_catalog->parts.size()) {
        QMessageBox::warning(this, tr("Keine Prüfung möglich"),
                             tr("Der Katalog deklariert keinen Prüfungsumfang. Bitte einen "
                                "Katalog der Klassen N, E oder A öffnen."));
        return false;
    }
    for (int questions : options.questionsPerPart) {
        if (questions <= 0) {
            QMessageBox::warning(this, tr("Keine Prüfung möglich"),
                                 tr("Mindestens ein Prüfungsteil deklariert keine Fragenanzahl."));
            return false;
        }
    }

    const quint32 seed = QRandomGenerator::global()->generate();
    const ExamPaper paper = PaperBuilder::build(*m_catalog, m_scheduler, options, seed);
    m_session.emplace(*m_catalog, paper);
    m_current = 0;
    m_clock.start();
    m_timer->start();
    showCurrentQuestion();
    return true;
}

bool ExamWidget::startTraining(int questionCount)
{
    if (!m_catalog || m_catalog->activeQuestionCount() <= 0)
        return false;
    PaperBuilder::Options options;
    options.questionCount = questionCount;
    const quint32 seed = QRandomGenerator::global()->generate();
    const ExamPaper paper = PaperBuilder::build(*m_catalog, m_scheduler, options, seed);
    if (paper.questions.isEmpty())
        return false;
    m_session.emplace(*m_catalog, paper);
    m_current = 0;
    m_timer->stop();
    showCurrentQuestion();
    return true;
}

void ExamWidget::showCurrentQuestion()
{
    if (!m_session || m_current >= m_session->count())
        return;

    const Question *question = m_session->questionAt(m_current);
    if (!question)
        return;

    clearQuestionArea();

    updateHeader();

    m_question->setText(plainTextOf(question));
    m_question->setVisible(true);

    const QPixmap questionImage = renderImage(question->image);
    if (!questionImage.isNull()) {
        auto *label = new QLabel(m_answerArea);
        label->setPixmap(questionImage);
        label->setAlignment(Qt::AlignLeft);
        m_answerLayout->addWidget(label);
    }

    const QVector<int> &order = m_session->answerOrderAt(m_current);

    // Adds the illustration of one answer, if it has one, above its text.
    const auto addAnswerImage = [this, &order, question](int answerIndex) {
        if (answerIndex < 0 || answerIndex >= question->answers.size())
            return;
        const QPixmap image = renderImage(question->answers.at(answerIndex).image);
        if (image.isNull())
            return;
        auto *label = new QLabel(m_answerArea);
        label->setPixmap(image);
        label->setAlignment(Qt::AlignLeft);
        label->setIndent(24);
        m_answerLayout->addWidget(label);
    };

    // The source stores answer texts with stray leading spaces, which would
    // show up as a gap behind the selection control.
    const auto answerText = [&order, question](int position) {
        const int answerIndex = order.value(position, -1);
        if (answerIndex >= 0 && answerIndex < question->answers.size())
            return question->answers.at(answerIndex).text.trimmed();
        return tr("Antwort %1").arg(position + 1);
    };

    if (question->isMultipleChoice()) {
        for (int position = 0; position < order.size(); ++position) {
            auto *box = new QCheckBox;
            box->setToolTip(answerText(position));
            addAnswerImage(order.at(position));
            m_answerLayout->addWidget(makeAnswerRow(m_answerArea, box, answerText(position)));
            m_multipleChoice.append(box);
        }
        if (m_multipleChoice.isEmpty())
            return;

        // The learner may mark several answers; they count as correct when all
        // correct ones are marked and no wrong one is.
        connect(m_multipleChoice.first(), &QCheckBox::toggled, this, [this] {
            if (!m_session)
                return;
            int marked = 0;
            for (QCheckBox *box : std::as_const(m_multipleChoice)) {
                if (box->isChecked())
                    ++marked;
            }
            m_session->answer(m_current, marked > 0 ? 0 : -1);
            updateNavigation();
        });
    } else {
        m_singleChoice = new QButtonGroup(m_answerArea);
        m_singleChoice->setExclusive(true);
        for (int position = 0; position < order.size(); ++position) {
            auto *button = new QRadioButton;
            button->setToolTip(answerText(position));
            addAnswerImage(order.at(position));
            m_answerLayout->addWidget(makeAnswerRow(m_answerArea, button, answerText(position)));
            m_singleChoice->addButton(button, position);
        }
        connect(m_singleChoice, &QButtonGroup::buttonClicked, this,
                [this](QAbstractButton *button) {
                    if (m_session)
                        m_session->answer(m_current, m_singleChoice->id(button));
                    updateNavigation();
                });
    }

    // Restore a previous answer of this question, e.g. after going back.
    const int selected = m_session->selectedPosition(m_current);
    if (selected >= 0) {
        if (question->isMultipleChoice()) {
            const int correct = m_session->correctPositionAt(m_current);
            // Only the "answered" state can be restored without revealing the
            // solution, so a saved answer is shown as marked.
            if (selected < m_multipleChoice.size())
                m_multipleChoice.at(selected)->setChecked(true);
            Q_UNUSED(correct);
        } else if (m_singleChoice && selected < m_singleChoice->buttons().size()) {
            m_singleChoice->button(selected)->setChecked(true);
        }
    }

    // The rows keep their natural height; the remaining space stays empty
    // instead of stretching the answers apart.
    m_answerLayout->addStretch(1);
    activateAnswerArea();
    updateNavigation();
}

void ExamWidget::updateHeader()
{
    if (!m_session) {
        m_header->clear();
        return;
    }
    const ExamPaper &paper = m_session->paper();
    const QString letter = m_session->partLetterAt(m_current);
    if (paper.isFullExam()) {
        // The catalog title already begins with the letter of the
        // Prüfungsteil and ends with the authority it comes from. Neither is
        // repeated here, only the name of the part is shown.
        QString partName = m_session->partTitleAt(m_current);
        const int authority = partName.indexOf(QStringLiteral(" – "));
        if (authority > 0)
            partName = partName.left(authority);
        if (partName.startsWith(letter))
            partName = partName.mid(letter.size()).trimmed();
        // "V (Kenntnisse von Vorschriften)" becomes "Kenntnisse von Vorschriften".
        if (partName.startsWith(QLatin1Char('(')) && partName.endsWith(QLatin1Char(')'))) {
            partName = partName.mid(1, partName.size() - 2).trimmed();
        }
        m_header->setText(tr("Prüfungsteil %1 — %2  ·  Frage %3 von %4  ·  Bestehen ab %5 von %6 Punkten")
                              .arg(letter, partName)
                              .arg(m_current + 1)
                              .arg(m_session->count())
                              .arg(paper.passScore)
                              .arg(paper.totalPoints));
    } else {
        m_header->setText(tr("Training — Frage %1 von %2").arg(m_current + 1).arg(m_session->count()));
    }
}

void ExamWidget::updateNavigation()
{
    const bool running = m_session.has_value();
    const bool fullExam = running && m_session->paper().isFullExam();

    m_startExam->setEnabled(m_catalog && !running);
    m_startTraining->setEnabled(m_catalog && !running);
    m_back->setEnabled(running && m_current > 0);
    m_finish->setEnabled(running);
    m_next->setEnabled(running && m_session->count() > 0);
    if (m_startTraining)
        m_startTraining->setEnabled(m_catalog && !running);

    if (!running) {
        m_progress->setText(m_catalog
                                ? tr("%1 Fragen im Katalog verfügbar")
                                      .arg(m_catalog->activeQuestionCount())
                                : tr("Kein Katalog geladen"));
        return;
    }

    const int answered = m_session->answeredCount();
    const int total = m_session->count();
    if (fullExam) {
        m_progress->setText(tr("%1 von %2 beantwortet  ·  %3 offen")
                                .arg(answered)
                                .arg(total)
                                .arg(total - answered));
    } else {
        m_progress->setText(tr("%1 von %2 beantwortet").arg(answered).arg(total));
    }
    m_timerLabel->setText(fullExam
                              ? tr("Restzeit %1").arg(formatMinutesLeft(
                                    qint64(m_session->paper().totalMinutes) * 60 - m_clock.elapsed() / 1000))
                              : QString());
}

void ExamWidget::tick()
{
    if (!m_session || !m_session->paper().isFullExam())
        return;
    const qint64 left = qint64(m_session->paper().totalMinutes) * 60 - m_clock.elapsed() / 1000;
    if (left <= 0) {
        m_timer->stop();
        QMessageBox::information(this, tr("Zeit abgelaufen"),
                                 tr("Die Prüfungszeit ist um. Die Prüfung wird ausgewertet."));
        finishExam();
        return;
    }
    updateNavigation();
}

void ExamWidget::goToNextUnanswered()
{
    if (!m_session)
        return;
    const int next = m_session->nextUnanswered(m_current);
    if (next >= 0 && next != m_current)
        m_current = next;
    else if (m_current + 1 < m_session->count())
        ++m_current;
    showCurrentQuestion();
}

void ExamWidget::goBack()
{
    if (!m_session || m_current <= 0)
        return;
    --m_current;
    showCurrentQuestion();
}

void ExamWidget::applyResultToScheduler(const ExamResult &result)
{
    if (!m_session)
        return;
    for (int i = 0; i < m_session->count(); ++i) {
        AnswerOutcome outcome;
        outcome.questionId = m_session->questionAt(i)
                                 ? m_session->questionAt(i)->id
                                 : m_session->paper().questions.at(i).questionId;
        const int selected = m_session->selectedPosition(i);
        const int correct = m_session->correctPositionAt(i);
        outcome.correct = selected >= 0 && correct >= 0 && selected == correct;
        // An examination carries no self report. "Unknown" makes the scheduler
        // move a correctly answered question exactly one box.
        outcome.confidence = Confidence::Unknown;
        m_scheduler.record(outcome);
    }
    Q_UNUSED(result)
}

void ExamWidget::finishExam()
{
    if (!m_session)
        return;

    const int unanswered = m_session->count() - m_session->answeredCount();
    if (unanswered > 0) {
        const auto answer = QMessageBox::question(
            this, tr("Prüfung abschließen"),
            tr("%1 Fragen sind noch unbeantwortet und werden als falsch gewertet. "
               "Trotzdem auswerten?")
                .arg(unanswered),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    const ExamResult result = m_session->evaluate();
    applyResultToScheduler(result);
    emit examFinished(result);
    emit progressChanged(m_scheduler);

    m_session.reset();
    m_current = 0;
    m_timer->stop();
    clearQuestionArea();
    m_question->clear();
    updateHeader();
    updateNavigation();
}

} // namespace hrt

#include "ExamWidget.moc"