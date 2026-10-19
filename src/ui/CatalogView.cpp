// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "CatalogView.h"

#include "AppIcons.h"

#include "ImageLoader.h"
#include "QuestionText.h"

#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QSettings>
#include <QLabel>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace hrt {
namespace {

/// Ab welcher Stufe eine Frage als gefestigt gilt.
///
/// Dieselbe Zahl wie in LearningStatistics - zwei Schwellen im Programm
/// waeren zwei Antworten auf die Frage "ab wann gilt etwas als gelernt",
/// und die Antworten liefen auseinander, sobald eine davon sich aenderte.
constexpr int kGefestigtAb = Scheduler::MaxBox - 1;

constexpr int kColumnId = 0;
constexpr int kColumnText = 1;
constexpr int kColumnStage = 2;
constexpr int kColumnLastSeen = 3;
constexpr int kColumnDue = 4;

/// Which questions belong to which tree row. Resolving that from the tree
/// text would mean searching the whole catalog on every click.
using QuestionSets = QHash<QTreeWidgetItem *, QVector<const Question *>>;

QString stageName(const QuestionState &state)
{
    if (state.isNew())
        return QObject::tr("neu");
    switch (state.box) {
    case 1:
        return QObject::tr("Stufe 1 · ungesichert");
    case 2:
        return QObject::tr("Stufe 2 · wird gelernt");
    case 3:
        return QObject::tr("Stufe 3 · wird gefestigt");
    case 4:
        return QObject::tr("Stufe 4 · fast sicher");
    default:
        return QObject::tr("gefestigt");
    }
}

QString dateOrDash(const QDate &date)
{
    return date.isValid() ? date.toString(QStringLiteral("dd.MM.yyyy")) : QStringLiteral("–");
}

} // namespace

CatalogView::CatalogView(QWidget *parent)
    : QWidget(parent)
{
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("kapitelbaum"));
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Titel"), tr("Fragen")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tree->header()->setStretchLastSection(false);
    m_tree->setRootIsDecorated(true);

    m_questions = new QTableWidget(this);
    m_questions->setObjectName(QStringLiteral("fragenliste"));
    m_questions->setColumnCount(5);
    // The column names are the ones the 2015 trainer used, so that the view is
    // recognisable for anyone who learned with it.
    m_questions->setHorizontalHeaderLabels({tr("Kennung"), tr("Frage"), tr("Lernstufe"),
                                            tr("Letzte Abfrage"), tr("Wiedervorlage")});
    m_questions->verticalHeader()->setVisible(false);
    m_questions->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_questions->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_questions->setSelectionMode(QAbstractItemView::SingleSelection);
    m_questions->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_questions->horizontalHeader()->setStretchLastSection(false);
    m_questions->setWordWrap(false);

    m_detail = new QWidget(this);
    auto *detailLayout = new QVBoxLayout(m_detail);
    detailLayout->setContentsMargins(6, 6, 6, 6);
    m_detailHeading = new QLabel(m_detail);
    QFont headingFont = m_detailHeading->font();
    headingFont.setBold(true);
    m_detailHeading->setFont(headingFont);
    m_detailHeading->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailLayout->addWidget(m_detailHeading);
    m_detailHeading->hide();

    m_right = new QSplitter(Qt::Vertical, this);
    m_right->addWidget(m_questions);
    m_right->addWidget(m_detail);
    m_right->setStretchFactor(0, 1);
    m_right->setStretchFactor(1, 2);

    m_columns = new QSplitter(Qt::Horizontal, this);
    m_columns->addWidget(m_tree);
    m_columns->addWidget(m_right);
    m_columns->setStretchFactor(0, 0);
    m_columns->setStretchFactor(1, 1);
    m_columns->setSizes({320, 900});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_columns);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
            [this] { fillQuestionList(m_tree->currentItem()); });
    connect(m_questions, &QTableWidget::itemSelectionChanged, this,
            [this] { showSelectedQuestion(); });

    m_loadingLayout = true;
    makeColumnsAdjustable();
    restoreLayout();
    m_loadingLayout = false;
}

CatalogView::~CatalogView()
{
    // Ein Absturz, ein Neustart aus dem Terminal oder ein SIGTERM sollen
    // die eingestellten Spalten nicht wieder verwerfen.
    saveLayout();
}

void CatalogView::makeColumnsAdjustable()
{
    // Qt laesst eine Spalte nur ziehen, wenn ihre ResizeMode nicht Stretch
    // oder ResizeToContents ist. Beides stand vorher fest, damit die Ansicht
    // beim Oeffnen ordentlich aussah - auf Kosten der Hand, die sie
    // einrichten wollte.
    //
    // Die erste Breite einmalig aus dem Inhalt holen; ab da gehoert die
    // Spalte dem Lernenden.
    m_questions->resizeColumnsToContents();
    if (m_questions->columnWidth(kColumnText) < 200)
        m_questions->setColumnWidth(kColumnText, 200);
    // Im Baum traegt die erste Spalte die langen Kapitelnamen; ohne
    // Mindestbreite bliebe neben den Zahlen nur leeres Feld stehen.
    m_tree->resizeColumnToContents(0);
    if (m_tree->columnWidth(0) < 220)
        m_tree->setColumnWidth(0, 220);

    connect(m_tree->header(), &QHeaderView::sectionResized, this, [this] { saveLayout(); });
    connect(m_questions->horizontalHeader(), &QHeaderView::sectionResized, this,
            [this] { saveLayout(); });
    connect(m_columns, &QSplitter::splitterMoved, this, [this](int, int) { saveLayout(); });
    connect(m_right, &QSplitter::splitterMoved, this, [this](int, int) { saveLayout(); });
}

void CatalogView::saveLayout()
{
    // Solange die Ansicht aufgebaut wird, schreibt sie ihren eigenen
    // Aufbau zurueck und ueberschreibt damit das gerade eingelesene Layout.
    // Ohne diese Bremse war die erste Ansicht nach dem Neustart wieder so
    // breit wie ihr Inhalt.
    if (m_loadingLayout)
        return;

    QSettings settings;
    // Der Zustand des Kopfes enthaelt Breiten, Reihenfolge und die
    // ResizeModes. Das ueberlebt auch eine Aenderung der Spaltenzahl, weil
    // Qt unbenannte Abschnitte verwirft.
    settings.setValue(QStringLiteral("catalogView/kapitelbaum"), m_tree->header()->saveState());
    settings.setValue(QStringLiteral("catalogView/fragenliste"),
                      m_questions->horizontalHeader()->saveState());
    settings.setValue(QStringLiteral("catalogView/spalten"), m_columns->saveState());
    settings.setValue(QStringLiteral("catalogView/obenUnten"), m_right->saveState());
}

void CatalogView::restoreLayout()
{
    QSettings settings;
    const QByteArray tree = settings.value(QStringLiteral("catalogView/kapitelbaum")).toByteArray();
    if (!tree.isEmpty())
        m_tree->header()->restoreState(tree);
    const QByteArray questions =
        settings.value(QStringLiteral("catalogView/fragenliste")).toByteArray();
    if (!questions.isEmpty())
        m_questions->horizontalHeader()->restoreState(questions);
    const QByteArray columns = settings.value(QStringLiteral("catalogView/spalten")).toByteArray();
    if (!columns.isEmpty())
        m_columns->restoreState(columns);
    const QByteArray stacked =
        settings.value(QStringLiteral("catalogView/obenUnten")).toByteArray();
    if (!stacked.isEmpty())
        m_right->restoreState(stacked);
}

void CatalogView::setCatalog(const Catalog *catalog, const QString &catalogPath)
{
    m_catalog = catalog;
    m_catalogPath = catalogPath;
    m_states.clear();
    buildTree();
    clearDetail();
}

void CatalogView::setProgress(const QVector<QuestionState> &states)
{
    m_states.clear();
    m_states.reserve(states.size());
    for (const QuestionState &state : states)
        m_states.insert(state.questionId, state);

    // Die Symbole des Baumes tragen denselben Fortschritt, sie muessen also
    // bei jeder Antwort neu gesetzt werden - sonst zeigte der Baum weiter,
    // was beim letzten Klick galt.
    for (int top = 0; top < m_tree->topLevelItemCount(); ++top)
        paintStatusBelow(m_tree->topLevelItem(top));

    // The list carries the progress, so it has to be rebuilt.
    if (m_tree->currentItem())
        fillQuestionList(m_tree->currentItem());
}

void CatalogView::paintStatusBelow(QTreeWidgetItem *item)
{
    if (!item)
        return;
    // QHash::value gibt die Liste selbst zurueck, nicht einen Zeiger darauf -
    // ein Zeiger daraus waere ein Zeiger auf eine Kopie, die mit der Schleife
    // endet.
    const auto sitz = m_sets.constFind(item);
    if (sitz != m_sets.constEnd()) {
        const QVector<const Question *> &fragen = sitz.value();
        int beantwortet = 0;
        int gefestigt = 0;
        for (const Question *frage : fragen) {
            const auto sitz = m_states.constFind(frage->id);
            if (sitz == m_states.constEnd())
                continue;
            if (sitz->timesAnswered == 0)
                continue;
            ++beantwortet;
            if (sitz->box >= kGefestigtAb)
                ++gefestigt;
        }

        // Ohne eine einzige beantwortete Frage bleibt das Symbol farblos. Eine
        // rote Marke an einem Kapitel, das noch niemand gesehen hat, waere
        // eine Warnung fuer etwas, das nicht schiefgegangen ist.
        QString name = QStringLiteral("status-new");
        QString text;
        if (fragen.isEmpty()) {
            name.clear();
        } else if (beantwortet == 0) {
            name = QStringLiteral("status-new");
            text = tr("noch keine Frage bearbeitet");
        } else {
            const int anteil = gefestigt * 100 / fragen.size();
            name = anteil >= 60 ? QStringLiteral("status-high")
                                : (anteil >= 25 ? QStringLiteral("status-mid")
                                                : QStringLiteral("status-low"));
            text = tr("%1 gefestigt, %2 bearbeitet (von %3)")
                       .arg(gefestigt)
                       .arg(beantwortet)
                       .arg(fragen.size());
        }
        item->setIcon(0, name.isEmpty() ? QIcon() : AppIcons::get(name));
        item->setToolTip(0, name.isEmpty() ? QString() : text);
    }

    for (int i = 0; i < item->childCount(); ++i)
        paintStatusBelow(item->child(i));
}

void CatalogView::buildTree()
{
    // m_tree->clear() loest itemSelectionChanged aus, und das faellt ueber
    // den Signalweg in fillQuestionList() zurueck. Zu diesem Zeitpunkt sind
    // die alten Baumzeilen schon geloescht und m_sets zeigt noch auf sie -
    // der Weg dorthin laeuft also ueber freigegebenen Speicher. Erst die
    // Zuordnung loesen, dann den Baum leeren, und waehrenddessen die Signale
    // sperren, damit gar nichts zurueckkommt.
    m_sets.clear();
    m_tree->blockSignals(true);
    m_tree->clear();
    m_tree->blockSignals(false);
    m_questions->setRowCount(0);
    clearDetail();
    if (!m_catalog)
        return;

    for (const Part &part : m_catalog->parts) {
        auto *partItem = new QTreeWidgetItem(m_tree);
        partItem->setText(0, part.title);
        partItem->setToolTip(0, tr("Prüfungsteil, %1 Fragen in der Prüfung, %2 im Katalog")
                                  .arg(part.selection.questions)
                                  .arg(partAvailable(part)));
        partItem->setText(1, QString::number(partAvailable(part)));
        m_sets.insert(partItem, questionsBelow(part));

        for (const Section &section : part.sections) {
            auto *sectionItem = new QTreeWidgetItem(partItem);
            sectionItem->setText(0, section.title);
            sectionItem->setText(1, QString::number(questionsBelow(section).size()));
            m_sets.insert(sectionItem, questionsBelow(section));

            for (const Chapter &chapter : section.chapters) {
                auto *chapterItem = new QTreeWidgetItem(sectionItem);
                chapterItem->setText(0, chapter.title);
                QVector<const Question *> chapterQuestions;
                for (const Question &question : chapter.questions)
                    chapterQuestions.append(&question);
                chapterItem->setText(1, QString::number(chapterQuestions.size()));
                m_sets.insert(chapterItem, chapterQuestions);
            }
        }
        // Erst nach den Kindern: paintStatusBelow() laeuft in die Kinder
        // hinein, und vor diesem Punkt gibt es keine.
        paintStatusBelow(partItem);
        partItem->setExpanded(true);
    }

    if (m_tree->topLevelItemCount() > 0)
        m_tree->setCurrentItem(m_tree->topLevelItem(0));
}

QVector<const Question *> CatalogView::questionsBelow(const Part &part) const
{
    QVector<const Question *> questions;
    for (const Section &section : part.sections)
        for (const Chapter &chapter : section.chapters)
            for (const Question &question : chapter.questions)
                if (question.isActive())
                    questions.append(&question);
    return questions;
}

QVector<const Question *> CatalogView::questionsBelow(const Section &section) const
{
    QVector<const Question *> questions;
    for (const Chapter &chapter : section.chapters)
        for (const Question &question : chapter.questions)
            if (question.isActive())
                questions.append(&question);
    return questions;
}

int CatalogView::partAvailable(const Part &part) const
{
    int count = 0;
    for (const Section &section : part.sections)
        for (const Chapter &chapter : section.chapters)
            for (const Question &question : chapter.questions)
                if (question.isActive())
                    ++count;
    return count;
}

void CatalogView::fillQuestionList(QTreeWidgetItem *item)
{
    m_questions->setRowCount(0);
    clearDetail();
    if (!m_catalog || !item)
        return;

    const QVector<const Question *> questions = m_sets.value(item);
    m_questions->setRowCount(questions.size());
    for (int row = 0; row < questions.size(); ++row) {
        const Question *question = questions.at(row);
        const QuestionState state = m_states.value(question->id);

        auto *id = new QTableWidgetItem(question->id);
        m_questions->setItem(row, kColumnId, id);

        auto *text = new QTableWidgetItem(question->text.simplified());
        text->setToolTip(question->text);
        m_questions->setItem(row, kColumnText, text);

        m_questions->setItem(
            row, kColumnStage,
            new QTableWidgetItem(state.timesAnswered == 0 ? tr("unbeantwortet")
                                                          : stageName(state)));
        m_questions->setItem(row, kColumnLastSeen,
                             new QTableWidgetItem(dateOrDash(state.lastSeen)));
        m_questions->setItem(row, kColumnDue,
                             new QTableWidgetItem(state.dueOn.isValid()
                                                       ? dateOrDash(state.dueOn)
                                                       : tr("sofort")));
    }
    // Kein resizeColumnsToContents() und kein Stretch hier: beides hat die
    // Frage ueberschrieben, sobald eine andere Zeile eingetragen wurde, und
    // damit jede von Hand eingestellte Spaltenbreite wieder zerschossen.
    // Die Breiten kommen aus makeColumnsAdjustable() und ueberleben einen
    // Katalogwechsel.
}

void CatalogView::showSelectedQuestion()
{
    const int row = m_questions->currentRow();
    if (row < 0 || !m_catalog)
        return;
    QTableWidgetItem *idItem = m_questions->item(row, kColumnId);
    if (!idItem)
        return;
    const QString id = idItem->text();

    for (const Part &part : m_catalog->parts) {
        for (const Section &section : part.sections) {
            for (const Chapter &chapter : section.chapters) {
                for (const Question &question : chapter.questions) {
                    if (question.id == id) {
                        showQuestionDetail(question);
                        return;
                    }
                }
            }
        }
    }
}

void CatalogView::showQuestionDetail(const Question &question)
{
    // The pane is rebuilt from scratch; a question may carry an image, four
    // answer images and a solution hint, so keeping stale widgets around would
    // need more care than it saves.
    clearDetail();
    auto *layout = qobject_cast<QVBoxLayout *>(m_detail->layout());

    m_detailHeading->setText(tr("%1 — %2")
                                 .arg(question.id,
                                      question.isMultipleChoice()
                                          ? tr("Mehrfachauswahl")
                                          : tr("Einfachauswahl")));
    m_detailHeading->show();

    auto *text = new QLabel(m_detail);
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    setQuestionText(text, question.text, question.textFormat);
    layout->addWidget(text);

    const QDir catalogDir(QFileInfo(m_catalogPath).absolutePath());
    const QPixmap image = images::load(catalogDir, question.image);
    if (!image.isNull()) {
        auto *imageLabel = new QLabel(m_detail);
        imageLabel->setPixmap(image);
        imageLabel->setAlignment(Qt::AlignLeft);
        layout->addWidget(imageLabel);
    }

    auto *answersBox = new QGroupBox(tr("Antworten"), m_detail);
    auto *answersLayout = new QVBoxLayout(answersBox);
    const QVector<int> correct = question.correctAnswerIndexes();

    for (int i = 0; i < question.answers.size(); ++i) {
        const Answer &answer = question.answers.at(i);
        auto *row = new QWidget(answersBox);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(2, 2, 2, 2);

        auto *badge = new QLabel(QString(QChar('A' + i)), row);
        badge->setMinimumWidth(20);
        badge->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        badge->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rowLayout->addWidget(badge, 0);

        auto *answerText = new QLabel(row);
        answerText->setWordWrap(true);
        answerText->setTextInteractionFlags(Qt::TextSelectableByMouse);
        setQuestionText(answerText, answer.text.simplified(), question.textFormat);
        rowLayout->addWidget(answerText, 1);

        const QPixmap answerImage = images::load(catalogDir, answer.image);
        if (!answerImage.isNull()) {
            auto *imageLabel = new QLabel(row);
            imageLabel->setPixmap(answerImage);
            imageLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            rowLayout->addWidget(imageLabel, 0);
        }

        if (correct.contains(i)) {
            auto *mark = new QLabel(tr("richtig"), row);
            mark->setStyleSheet(QStringLiteral("color: #2e7d32; font-weight: bold;"));
            rowLayout->addWidget(mark, 0);
        }
        answersLayout->addWidget(row);
    }
    layout->addWidget(answersBox);

    if (!question.explanation.isEmpty()) {
        auto *hintBox = new QGroupBox(tr("Lösungshinweis"), m_detail);
        auto *hintLayout = new QVBoxLayout(hintBox);
        auto *hint = new QLabel(hintBox);
        hint->setWordWrap(true);
        hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
        setQuestionText(hint, question.explanation, question.textFormat);
        hintLayout->addWidget(hint);
        layout->addWidget(hintBox);
    }

    auto *progressBox = new QGroupBox(tr("Lernfortschritt"), m_detail);
    auto *progressLayout = new QVBoxLayout(progressBox);
    auto *progress = new QLabel(progressBox);
    progress->setWordWrap(true);
    const QuestionState state = m_states.value(question.id);
    if (state.timesAnswered == 0) {
        progress->setText(tr("Diese Frage wurde noch nie beantwortet."));
    } else {
        progress->setText(tr("%1 von %2 Beantwortungen richtig (Trefferquote %3 %). "
                             "Stufe: %4. Zuletzt beantwortet: %5. Nächste Wiederholung: %6.")
                              .arg(state.timesCorrect)
                              .arg(state.timesAnswered)
                              .arg(state.timesAnswered > 0
                                       ? QString::number(100.0 * state.timesCorrect
                                                          / state.timesAnswered,
                                                         'f', 1)
                                       : QStringLiteral("0,0"))
                              .arg(stageName(state))
                              .arg(dateOrDash(state.lastSeen))
                              .arg(state.dueOn.isValid() ? dateOrDash(state.dueOn)
                                                         : tr("sofort")));
    }
    progressLayout->addWidget(progress);
    layout->addWidget(progressBox);
    layout->addStretch(1);
}

void CatalogView::clearDetail()
{
    // Only the widgets added after the heading are removed.
    auto *layout = qobject_cast<QVBoxLayout *>(m_detail->layout());
    while (layout->count() > 1) {
        QLayoutItem *item = layout->takeAt(1);
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    m_detailHeading->setText(QString());
    m_detailHeading->hide();
}

bool CatalogView::showQuestion(const QString &questionId)
{
    for (int row = 0; row < m_questions->rowCount(); ++row) {
        QTableWidgetItem *item = m_questions->item(row, kColumnId);
        if (item && item->text() == questionId) {
            m_questions->selectRow(row);
            return true;
        }
    }
    return false;
}

} // namespace hrt

#include "CatalogView.moc"