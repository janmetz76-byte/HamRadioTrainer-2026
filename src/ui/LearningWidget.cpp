// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "LearningWidget.h"

#include "SectionBar.h"
#include "learning/StudyPlan.h"

#include "AnswerView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include "AppIcons.h"

#include <QPushButton>
#include <QSettings>
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
    return QObject::tr("Stufe %1: richtig beantwortet, aber noch nicht gefestigt. Eine "
                       "sichere richtige Antwort geht drei Stufen höher, eine unsichere "
                       "eine Stufe, eine geratene bleibt auf ihrer Stufe. Jede falsche "
                       "Antwort geht zwei Stufen zurück. Mit steigender Stufe wird die "
                       "Frage immer seltener abgefragt.")
        .arg(box);
}

/// What the exam date asks of today, in one or two sentences.
QString planText(const StudyPlan &plan)
{
    const QString readiness =
        QObject::tr("Prüfungsreife: %1 Prozent der Fragen gefestigt.").arg(plan.readiness(), 0, 'f', 0);
    if (plan.fresh == 0 && plan.newToday == 0)
        return QObject::tr("Alle Fragen sind mindestens einmal gelernt. Jetzt wiederholen, "
                           "was noch nicht sitzt. %1")
            .arg(readiness);
    if (plan.finalPhase)
        return QObject::tr("Schlussphase vor der Prüfung: Unsicheres und Falsches kommt zuerst, "
                           "danach die %1 noch nie gefragten Fragen. %2")
            .arg(plan.fresh)
            .arg(readiness);
    if (plan.newRemainingToday() == 0)
        return QObject::tr("Tagesziel erreicht: %1 neue Fragen heute. Wiederholen lohnt sich "
                           "trotzdem. %2")
            .arg(plan.newToday)
            .arg(readiness);
    return QObject::tr("Tagesziel: %1 neue Fragen, davon %2 geschafft. So sind bis eine Woche "
                       "vor der Prüfung alle Fragen gelernt. %3")
        .arg(plan.dailyNewTarget)
        .arg(plan.newToday)
        .arg(readiness);
}

} // namespace

LearningWidget::LearningWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);

    // Der Ueberblick steht nur zwischen den Runden da. Waehrend einer Runde
    // bekommt die Frage die ganze Hoehe: sonst blieb ihr bei normaler
    // Fenstergroesse ein Streifen, in dem gerade die erste Antwort Platz hatte.
    m_overview = new QWidget(this);
    m_overview->setObjectName(QStringLiteral("lernueberblick"));
    auto *overviewLayout = new QVBoxLayout(m_overview);
    overviewLayout->setContentsMargins(0, 0, 0, 0);

    m_summary = new QLabel(this);
    QFont summaryFont = m_summary->font();
    summaryFont.setBold(true);
    m_summary->setFont(summaryFont);
    m_summary->setWordWrap(true);
    overviewLayout->addWidget(new SectionBar(tr("Lernfortschritt"), this));
    overviewLayout->addWidget(m_summary);
    m_plan = new QLabel(this);
    m_plan->setObjectName(QStringLiteral("lernplan"));
    m_plan->setWordWrap(true);
    m_plan->hide();
    overviewLayout->addWidget(m_plan);

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
    overviewLayout->addWidget(m_boxes);

    // Wer "Ordner 1 bis 3" nicht kennt, kann die Anzeige nicht deuten. Ein
    // Satz darueber kostet nichts und beantwortet die Frage.
    auto *legend = m_legend = new QLabel(
        tr("Lernstufen nach Leitner: eine sichere richtige Antwort hebt die Frage drei "
           "Stufen, eine unsichere eine Stufe, eine geratene gar nicht. Eine falsche Antwort "
           "drückt sie zwei Stufen zurück. Je höher die Stufe, desto seltener wird die Frage "
           "wiederholt. Die Stufe gefestigt zählt als bekannt."),
        this);
    legend->setWordWrap(true);
    legend->setStyleSheet(QStringLiteral("color: palette(mid);"));
    overviewLayout->addWidget(legend);

    m_parts = new QTableWidget(0, 6, this);
    m_parts->setObjectName(QStringLiteral("fortschrittJePruefungsteil"));
    m_parts->setHorizontalHeaderLabels({tr("Prüfungsteil"), tr("Fragen"), tr("beantwortet"),
                                         tr("gefestigt"), tr("wackelig"), tr("Trefferquote")});
    m_parts->verticalHeader()->setVisible(false);
    m_parts->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_parts->setSelectionMode(QAbstractItemView::NoSelection);
    m_parts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_parts->setMaximumHeight(200);
    overviewLayout->addWidget(m_parts);
    layout->addWidget(m_overview);

    // Das Kästchen mit Rahmen und die graue Leiste würden zweimal
    // "Übungsrunde" sagen. Die Leiste ersetzt den Rahmen; sie sitzt in
    // derselben Zeile, damit die Leiste nicht allein steht.
    auto *options = new QWidget(this);
    options->setObjectName(QStringLiteral("uebungsrunde"));
    m_options = options;
    m_optionsGrid = new QGridLayout(options);
    m_roundSizeLabel = new QLabel(tr("Fragen:"), options);
    m_roundSize = new QSpinBox(options);
    m_roundSize->setRange(1, 200);
    m_roundSize->setValue(20);
    m_onlyDue = new QCheckBox(tr("nur fällige"), options);
    m_onlyDue->setToolTip(tr("Nur Wiederholungen, deren Fälligkeit erreicht ist."));
    m_onlyShaky = new QCheckBox(tr("nur wackelige"), options);
    m_onlyShaky->setToolTip(tr("Nur Fragen, die zuletzt falsch beantwortet oder geraten wurden."));
    // Wie im Trainer von 2015 abschaltbar: wer ohne Aufloesung durch die
    // Runde gehen will, erfaehrt erst am Ende, wie viele Antworten stimmten.
    m_showSolution = new QCheckBox(tr("Lösung nach jeder Antwort zeigen"), options);
    m_showSolution->setToolTip(tr("Nach der Selbsteinschätzung zeigen, ob die Antwort richtig "
                                  "war und welche Antwort richtig ist."));
    {
        QSettings settings;
        m_showSolution->setChecked(
            settings.value(QStringLiteral("learnview/showSolution"), true).toBool());
    }
    connect(m_showSolution, &QCheckBox::toggled, this, [](bool on) {
        QSettings settings;
        settings.setValue(QStringLiteral("learnview/showSolution"), on);
    });
    m_partLabel = new QLabel(tr("Prüfungsteil:"), options);
    m_partFilter = new QComboBox(options);
    m_start = new QPushButton(AppIcons::get(QStringLiteral("play")),
                              tr("Runde starten"), options);
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
    // would be treated like a question that was known. It is also what hands
    // the answer in: the learner judges their own answer before the solution
    // is shown, as in the 2015 trainer.
    m_confidenceGrid = new QGridLayout;
    m_confidenceLabel = new QLabel(tr("Wie sicher war die Antwort?"), this);
    m_confident = new QPushButton(AppIcons::get(QStringLiteral("confident")),
                                  tr("Sicher"), this);
    m_unsure = new QPushButton(AppIcons::get(QStringLiteral("unsure")),
                               tr("Unsicher"), this);
    m_guessing = new QPushButton(AppIcons::get(QStringLiteral("guessed")),
                                 tr("Geraten"), this);
    m_nextQuestion = new QPushButton(AppIcons::get(QStringLiteral("next")),
                                     tr("Nächste Frage"), this);
    layout->addLayout(m_confidenceGrid);
    arrangeControls();

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

void LearningWidget::setCompact(bool compact)
{
    if (compact == m_compact)
        return;
    m_compact = compact;
    arrangeControls();
    // Nur die Sichtbarkeit nachziehen, nicht showCurrent(): das liefe auch
    // vor dem ersten Katalog und sperrte dann "Runde starten" fuer immer.
    const bool running = m_session.has_value() && m_index < m_session->count();
    m_options->setVisible(!running || !m_compact);
}

void LearningWidget::arrangeControls()
{
    // Breit steht alles in je einer Zeile. Schmal - auf dem Handy im
    // Hochformat - waeren die Beschriftungen bis zur Unkenntlichkeit
    // abgeschnitten ("Frag", "nu", "de s"); dort bekommt jede Einstellung
    // ihre eigene Zeile und die Selbsteinschaetzung grosse Knoepfe.
    const auto clear = [](QGridLayout *grid) {
        while (QLayoutItem *item = grid->takeAt(0))
            delete item;
        for (int column = 0; column < grid->columnCount(); ++column)
            grid->setColumnStretch(column, 0);
    };
    clear(m_optionsGrid);
    clear(m_confidenceGrid);

    if (m_compact) {
        m_optionsGrid->addWidget(m_roundSizeLabel, 0, 0);
        m_optionsGrid->addWidget(m_roundSize, 0, 1);
        m_optionsGrid->addWidget(m_onlyDue, 1, 0, 1, 2);
        m_optionsGrid->addWidget(m_onlyShaky, 2, 0, 1, 2);
        m_optionsGrid->addWidget(m_showSolution, 3, 0, 1, 2);
        m_optionsGrid->addWidget(m_partLabel, 4, 0);
        m_optionsGrid->addWidget(m_partFilter, 4, 1);
        m_optionsGrid->addWidget(m_start, 5, 0, 1, 2);
        m_optionsGrid->setColumnStretch(1, 1);

        m_confidenceGrid->addWidget(m_confidenceLabel, 0, 0, 1, 3);
        m_confidenceGrid->addWidget(m_confident, 1, 0);
        m_confidenceGrid->addWidget(m_unsure, 1, 1);
        m_confidenceGrid->addWidget(m_guessing, 1, 2);
        m_confidenceGrid->addWidget(m_nextQuestion, 2, 0, 1, 3);
    } else {
        int column = 0;
        for (QWidget *widget : {static_cast<QWidget *>(m_roundSizeLabel),
                                static_cast<QWidget *>(m_roundSize),
                                static_cast<QWidget *>(m_onlyDue),
                                static_cast<QWidget *>(m_onlyShaky),
                                static_cast<QWidget *>(m_showSolution),
                                static_cast<QWidget *>(m_partLabel),
                                static_cast<QWidget *>(m_partFilter)})
            m_optionsGrid->addWidget(widget, 0, column++);
        m_optionsGrid->setColumnStretch(column++, 1);
        m_optionsGrid->addWidget(m_start, 0, column);

        column = 0;
        for (QWidget *widget : {static_cast<QWidget *>(m_confidenceLabel),
                                static_cast<QWidget *>(m_confident),
                                static_cast<QWidget *>(m_unsure),
                                static_cast<QWidget *>(m_guessing)})
            m_confidenceGrid->addWidget(widget, 0, column++);
        m_confidenceGrid->setColumnStretch(column++, 1);
        m_confidenceGrid->addWidget(m_nextQuestion, 0, column);
    }

    // Lernstufen und Teiltabelle passen schmal weder in die Breite noch in die
    // Hoehe; sie drueckten die Seite ueber den Bildschirmrand. Der Satz
    // darueber bleibt, die Tabellen zeigt die Ansicht "Statistik" vollstaendig.
    m_boxes->setVisible(!m_compact);
    m_legend->setVisible(!m_compact);
    m_parts->setVisible(!m_compact);

    // Auf dem Handy wird getippt, nicht gezielt geklickt.
    const int height = m_compact ? 48 : 0;
    for (QPushButton *button : {m_start, m_confident, m_unsure, m_guessing, m_nextQuestion})
        button->setMinimumHeight(height);
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
    m_overview->show();
    refreshPartFilter();
    refresh();
}

void LearningWidget::setProgress(const QVector<QuestionState> &states)
{
    m_scheduler = Scheduler(states, m_scheduler.examDate());
    refresh();
}

void LearningWidget::setExamDate(const QDate &date)
{
    m_scheduler.setExamDate(date);
    refresh();
}

void LearningWidget::clearProgress()
{
    m_scheduler.reset();
    refresh();
}

void LearningWidget::setSelectedParts(const QSet<int> &parts)
{
    m_selectedParts = parts;
    refreshPartFilter();
    refresh();
}

void LearningWidget::refreshPartFilter()
{
    const QString previous = m_partFilter->currentData().toString();
    m_partFilter->clear();
    m_partFilter->addItem(tr("alle"), QString());
    if (m_catalog) {
        // Nur die Teile, fuer die gelernt wird: wer von N nach E aufsteigt,
        // soll Vorschriften und Betriebstechnik nicht wieder angeboten bekommen.
        const QSet<int> selected = effectiveParts(*m_catalog, m_selectedParts);
        for (int i = 0; i < m_catalog->parts.size(); ++i) {
            if (!selected.contains(i))
                continue;
            const Part &part = m_catalog->parts.at(i);
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
        m_plan->hide();
        return;
    }

    const LearningStatistics statistics =
        computeStatistics(*m_catalog, m_scheduler, QDate::currentDate(), m_selectedParts);

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

    const StudyPlan plan = computeStudyPlan(statistics, m_scheduler.examDate());
    m_plan->setVisible(plan.active);
    if (plan.active)
        m_plan->setText(planText(plan));

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
    if (!part.isEmpty()) {
        options.parts = {part};
    } else if (m_catalog) {
        // "alle" heisst: alle gewaehlten Teile, nicht der ganze Katalog.
        const QSet<int> selected = effectiveParts(*m_catalog, m_selectedParts);
        for (int i = 0; i < m_catalog->parts.size(); ++i) {
            if (selected.contains(i))
                options.parts.append(examPartLetter(m_catalog->parts.at(i)));
        }
    }
    return options;
}

void LearningWidget::startRound()
{
    if (!m_catalog)
        return;

    // Ein Zufallswert je Runde: er mischt die Reihenfolge der Fragen gleicher
    // Prioritaet und die Antworten. Nie 0, das hiesse "feste Reihenfolge".
    const quint32 seed = QRandomGenerator::global()->bounded(1u, 0xffffffffu);
    StudyOptions options = currentOptions();
    options.shuffleSeed = seed;
    StudySession session(*m_catalog, &m_scheduler, options);
    if (session.count() == 0) {
        m_progress->setText(tr("Zu dieser Auswahl gibt es nichts zu üben."));
        return;
    }

    m_session.emplace(std::move(session));
    m_index = 0;
    m_correct = 0;
    m_seed = seed;
    m_explanation->clear();
    showCurrent();
}

void LearningWidget::showCurrent()
{
    const bool running = m_session.has_value() && m_index < m_session->count();

    m_overview->setVisible(!running);
    // Schmal treten auch die Einstellungen der Runde zurueck; sie sind
    // waehrend der Runde ohnehin gesperrt, und die Frage braucht den Platz.
    m_options->setVisible(!running || !m_compact);
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
    m_explanation->clear();

    m_progress->setText(tr("Frage %1 von %2  ·  Prüfungsteil %3")
                            .arg(m_index + 1)
                            .arg(m_session->count())
                            .arg(m_session->partLetterOf(id)));

    m_answers->setQuestion(question, PaperBuilder::shuffledAnswerOrder(*question, m_seed, id),
                           QDir(m_catalogDir));
}

void LearningWidget::onAnswerSelected(int position)
{
    // Die Auswahl allein verraet noch nichts. Erst die Selbsteinschaetzung
    // gibt die Antwort ab - wer die Loesung schon sieht, schaetzt nicht mehr
    // sich selbst ein, sondern nur noch, ob er richtig lag.
    if (!m_session || m_pendingQuestionId.isEmpty())
        return;
    for (QPushButton *button : {m_confident, m_unsure, m_guessing})
        button->setEnabled(position >= 0);
}

void LearningWidget::rateConfidence(int confidence)
{
    if (!m_session || m_pendingQuestionId.isEmpty())
        return;

    const int position = m_answers->selection();
    const Question *question = m_byId.value(m_pendingQuestionId, nullptr);
    if (!question || position < 0)
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

    m_session->record(m_pendingQuestionId, correct, static_cast<Confidence>(confidence));
    m_pendingQuestionId.clear();
    emit progressChanged(m_scheduler);
    refresh();

    if (!m_showSolution->isChecked()) {
        ++m_index;
        showCurrent();
        return;
    }

    // Erst jetzt die Aufloesung, wie im Trainer von 2015: "Antwort B ist
    // leider falsch. Richtig ist C." Der Buchstabe ist der angezeigte, nicht
    // der aus der Quelle - sonst passte er nicht zur gemischten Reihenfolge.
    const QStringList letters = m_answers->badges();
    const QString chosen = letters.value(position);
    const QString right = letters.value(correctPosition);
    m_answers->reveal(correctPosition, position);
    QString feedback = correct ? tr("Antwort %1 ist richtig.").arg(chosen)
                               : tr("Antwort %1 ist leider falsch. Richtig ist %2.")
                                     .arg(chosen, right);
    if (!question->explanation.isEmpty())
        feedback += QLatin1Char('\n') + question->explanation;
    m_explanation->setText(feedback);

    for (QPushButton *button : {m_confident, m_unsure, m_guessing})
        button->setEnabled(false);
    m_nextQuestion->setEnabled(true);
    m_nextQuestion->setFocus();
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
    m_overview->show();
    refresh();
}

} // namespace hrt