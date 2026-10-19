// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "LearningWidget.h"

#include "SectionBar.h"

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
#include "AppIcons.h"

#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QApplication>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QVBoxLayout>

namespace hrt {
namespace {

/// Name of a Leitner stage.
///
/// The word "Ordner" told nobody anything: it names the container, not the
/// meaning. The 2015 trainer called the same thing a "Lernkartei" and spoke of
/// "Fragen der Stufe ...", and the learner is meant to understand the label,
/// so every stage gets a plain word next to the number.
QString boxLabel(int box)
{
    if (box <= 0)
        return QObject::tr("neu");
    if (box >= Scheduler::MaxBox)
        return QObject::tr("gefestigt");
    switch (box) {
    case 1:
        return QObject::tr("Stufe 1 · ungesichert");
    case 2:
        return QObject::tr("Stufe 2 · wird gelernt");
    case 3:
        return QObject::tr("Stufe 3 · wird gefestigt");
    default:
        return QObject::tr("Stufe %1 · fast sicher").arg(box);
    }
}

/// The picture of a stage.
///
/// One name for one stage, used both when the row is built and when it is
/// filled in, so a stage can never end up with the picture of its neighbour.
/// Malt eine Stufenzelle: Bild oben in der Mitte, Text darunter.
///
/// Der Zellensymbol-Eigenschaft einer Tabelle malen Qt immer links vor dem
/// Text. Bei einer zentrierten zweizeiligen Zelle sitzt das Bild dann am
/// Rand und der Text in der Mitte - zwei Dinge an zwei Orten. Der Delegate
/// setzt beide an dieselbe Stelle.
class BoxenZelle : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        // Nur der Hintergrund und die Auswahl kommen vom Stil. Text und Bild
        // werden vorher geleert, weil CE_ItemViewItem beides seinerseits
        // malt - links an der Zellkante statt mittig untereinander.
        opt.text.clear();
        opt.icon = QIcon();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        const QRect flaeche = opt.rect;
        const QIcon bild = index.data(Qt::DecorationRole).value<QIcon>();
        if (!bild.isNull()) {
            const QSize ziel(26, 26);
            const int x = flaeche.center().x() - ziel.width() / 2;
            painter->drawPixmap(QPoint(x, flaeche.top() + 6), bild.pixmap(ziel));
        }

        QFont schrift = qvariant_cast<QFont>(index.data(Qt::FontRole));
        if (schrift == QFont())
            schrift = opt.font;
        // Der Delegate schreibt in fremden Painter; ohne das bleibt die
        // Aenderung an diesem einen Bild haengen und faellt im Bildschirmfoto
        // der naechsten Zelle wieder auf.
        painter->save();
        painter->setFont(schrift);
        painter->setPen(opt.palette.color(QPalette::Text));
        QRect texte(flaeche);
        texte.setTop(flaeche.top() + 34);
        painter->drawText(texte, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                          index.data(Qt::DisplayRole).toString());
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QSize zwei(60, opt.fontMetrics.height() * 2 + 40);
        return zwei.expandedTo(QStyledItemDelegate::sizeHint(option, index));
    }
};

QString boxIcon(int box)
{
    const int stufe = qBound(0, box, Scheduler::MaxBox);
    return QStringLiteral("stufe-%1").arg(stufe);
}

/// Explains what a stage means and how the learner gets there.
QString boxDescription(int box)
{
    if (box <= 0)
        return QObject::tr("Neu: nie richtig beantwortet. Wird zuerst wiederholt.");
    if (box >= Scheduler::MaxBox)
        return QObject::tr("Gefestigt: mehrfach richtig in Folge. Wird nur selten "
                          "wiederholt und zählt als bekannt.");
    return QObject::tr("Stufe %1: richtig beantwortet, aber noch nicht gefestigt. Jede "
                       "richtige Antwort geht eine Stufe höher, jede falsche zwei "
                       "Stufen zurück. Mit steigender Stufe wird die Frage immer "
                       "seltener abgefragt.")
        .arg(box);
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
    layout->addWidget(new SectionBar(tr("Lernfortschritt"), this));
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
    m_boxes->verticalHeader()->setDefaultSectionSize(80);
    m_boxes->setFixedHeight(88);
    m_boxes->setItemDelegate(new BoxenZelle(m_boxes));
    for (int box = 0; box <= Scheduler::MaxBox; ++box) {
        auto *cell = new QTableWidgetItem(boxLabel(box));
        cell->setToolTip(boxDescription(box));
        cell->setTextAlignment(Qt::AlignCenter);
        // Das Bild wird beim Fuellen noch einmal gesetzt: eine Zelle ohne Zahl
        // sieht sonst nach einer kaputten Zeile aus, nicht nach einer leeren.
        cell->setIcon(AppIcons::get(boxIcon(box)));
        m_boxes->setItem(0, box, cell);
    }
    m_boxes->setToolTip(boxDescription(1));
    layout->addWidget(m_boxes);

    // Wer "Ordner 1 bis 3" nicht kennt, kann die Anzeige nicht deuten. Ein
    // Satz darueber kostet nichts und beantwortet die Frage.
    auto *legend = new QLabel(
        tr("Lernstufen nach Leitner: eine richtige Antwort hebt die Frage eine Stufe, eine "
           "falsche drückt sie zwei Stufen zurück. Je höher die Stufe, desto seltener wird "
           "die Frage wiederholt. Die Stufe gefestigt zählt als bekannt."),
        this);
    legend->setWordWrap(true);
    legend->setStyleSheet(QStringLiteral("color: palette(mid);"));
    layout->addWidget(legend);

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

    // Das Kästchen mit Rahmen und die graue Leiste würden zweimal
    // "Übungsrunde" sagen. Die Leiste ersetzt den Rahmen; sie sitzt in
    // derselben Zeile, damit die Leiste nicht allein steht.
    auto *options = new QWidget(this);
    options->setObjectName(QStringLiteral("uebungsrunde"));
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
    m_start = new QPushButton(AppIcons::get(QStringLiteral("play")),
                              tr("Runde starten"), options);
    optionsLayout->addWidget(m_start);
    layout->addWidget(new SectionBar(tr("Übungsrunde"), this));
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
    auto *hinweisBalken = new SectionBar(tr("Lösungshinweise"), this);
    hinweisBalken->setObjectName(QStringLiteral("loesungshinweise"));
    layout->addWidget(hinweisBalken);
    layout->addWidget(m_explanation);

    // The self report. Without it a correctly answered but guessed question
    // would be treated like a question that was known.
    auto *confidenceRow = new QHBoxLayout;
    confidenceRow->addWidget(new QLabel(tr("Wie sicher war die Antwort?"), this));
    m_confident = new QPushButton(AppIcons::get(QStringLiteral("confident")),
                                  tr("Sicher"), this);
    m_unsure = new QPushButton(AppIcons::get(QStringLiteral("unsure")),
                               tr("Unsicher"), this);
    m_guessing = new QPushButton(AppIcons::get(QStringLiteral("guessed")),
                                 tr("Geraten"), this);
    for (QPushButton *button : {m_confident, m_unsure, m_guessing})
        confidenceRow->addWidget(button);
    confidenceRow->addStretch(1);
    m_nextQuestion = new QPushButton(AppIcons::get(QStringLiteral("next")),
                                     tr("Nächste Frage"), this);
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
        item->setIcon(AppIcons::get(boxIcon(box)));
        // Die Zelle traegt die Zahl, das Bild die Stufe. Beide duerfen sich
        // nicht die Farbe streitig machen: ein gruener Hintergrund unter einem
        // gelben Stufenbild las sich als mittlere Stufe, obwohl die Zelle
        // nichts ueber die Stufe sagt. Der Hintergrund bleibt deshalb grau
        // und wird nur dunkler, je mehr Fragen darin liegen - die Verteilung
        // bleibt so auf einen Blick lesbar, ohne die Bilder zu uebertoenen.
        const int share = statistics.answered > 0 ? count * 100 / statistics.answered : 0;
        const int ton = qBound(0, share, 100);
        const QColor grau(0xf2 - ton * 3 / 10, 0xf2 - ton * 3 / 10, 0xf2 - ton * 3 / 10);
        item->setBackground(grau);
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