// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "Trainer.h"

#include "ExamRound.h"
#include "StudyRound.h"

#include "core/CatalogLocator.h"
#include "core/CatalogXml.h"
#include "core/ExamPlan.h"
#include "core/LatexFormatter.h"
#include "learning/LearningStatistics.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace hrt {
namespace {

/// Question and answer texts as QML rich text. Plain texts are escaped, so a
/// "<" in a question cannot be read as markup.
QString richText(const QString &text, TextFormat format)
{
    const QString simplified = text.simplified();
    if (format == TextFormat::Latex)
        return LatexFormatter::toRichText(simplified);
    return simplified.toHtmlEscaped();
}

} // namespace

Trainer::Trainer(QObject *parent)
    : QObject(parent)
{
    m_study = new StudyRound(this);
    m_exam = new ExamRound(this);
    findCatalogs();

    // Dieselben Schluessel wie das Hauptfenster am Desktop: wer zwischen
    // beiden wechselt, findet dieselbe Wahl vor.
    QSettings settings;
    m_examDate = settings.value(QStringLiteral("startup/examDate")).toDate();
    const QString id = settings.value(QStringLiteral("startup/combination")).toString();
    const ExamCombination combination = examCombination(id);
    if (settings.value(QStringLiteral("startup/done")).toBool() && !combination.id.isEmpty()
        && m_catalogs.contains(combination.targetClass)
        && openCatalog(m_catalogs.value(combination.targetClass).path)) {
        m_combinationId = combination.id;
        m_setupDone = true;
    }
}

Trainer::~Trainer()
{
    saveProgress();
}

void Trainer::findCatalogs()
{
    const CatalogLocator::Result located = CatalogLocator::locate();
    if (located.paths.isEmpty()) {
        m_problem = located.problem;
        return;
    }
    for (const QString &path : located.paths) {
        Catalog catalog;
        QString error;
        if (!CatalogXml::load(path, &catalog, &error))
            continue;
        const QString examClass = examClassToString(catalog.metadata.examClass);
        if (m_catalogs.contains(examClass))
            continue;
        CatalogInfo info;
        info.path = path;
        for (const Part &part : catalog.parts) {
            int questions = 0;
            for (const Section &section : part.sections)
                questions += section.activeQuestionCount();
            info.questionsPerPart.append(questions);
        }
        m_catalogs.insert(examClass, info);
    }
}

bool Trainer::openCatalog(const QString &path)
{
    if (path == m_catalogPath)
        return true;

    Catalog catalog;
    QString error;
    if (!CatalogXml::load(path, &catalog, &error)) {
        m_problem = error;
        return false;
    }
    saveProgress();
    m_catalog = std::move(catalog);
    m_catalogPath = path;
    m_catalogDir = QFileInfo(path).absolutePath();

    // Ein geaenderter Katalog verwirft keinen Fortschritt. Am Desktop fragt
    // ein Dialog; auf dem Handy wird uebernommen, was noch passt - das ist
    // die Wahl, die keine Arbeit kostet. Die alte Datei bleibt als Sicherung.
    m_store = ProgressStore(ProgressStore::defaultPath(m_catalog));
    const ProgressLoad loaded = m_store.load(m_catalog);
    if (loaded.catalogChanged) {
        QString archiveError;
        m_store.archive(&archiveError);
    }
    m_scheduler = Scheduler(loaded.states);
    if (loaded.catalogChanged)
        saveProgress();

    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), path);
    m_study->reset();
    m_exam->reset();
    emit progressChanged();
    return true;
}

QString Trainer::combinationLabel() const
{
    return examCombination(m_combinationId).label;
}

QString Trainer::partsText() const
{
    QStringList names;
    for (int part : examCombination(m_combinationId).parts)
        names.append(examPartName(part));
    return names.join(QStringLiteral(", "));
}

int Trainer::daysToExam() const
{
    if (!m_examDate.isValid())
        return -1;
    return int(QDate::currentDate().daysTo(m_examDate));
}

QSet<int> Trainer::selectedParts() const
{
    const QVector<int> parts = examCombination(m_combinationId).parts;
    return QSet<int>(parts.begin(), parts.end());
}

QVariantMap Trainer::statistics() const
{
    QVariantMap result;
    if (m_catalogPath.isEmpty())
        return result;

    const LearningStatistics statistics =
        computeStatistics(m_catalog, m_scheduler, QDate::currentDate(), selectedParts());
    result.insert(QStringLiteral("questions"), statistics.questions);
    result.insert(QStringLiteral("answered"), statistics.answered);
    result.insert(QStringLiteral("fresh"), statistics.fresh);
    result.insert(QStringLiteral("shaky"), statistics.shaky);
    result.insert(QStringLiteral("mastered"), statistics.mastered);
    result.insert(QStringLiteral("due"), statistics.due);
    result.insert(QStringLiteral("coverage"), statistics.coverage());
    result.insert(QStringLiteral("accuracy"), statistics.accuracy());

    QVariantList boxes;
    for (int count : statistics.byBox)
        boxes.append(count);
    result.insert(QStringLiteral("boxes"), boxes);

    QVariantList parts;
    for (const PartProgress &part : statistics.parts) {
        parts.append(QVariantMap{
            {QStringLiteral("letter"), part.letter},
            {QStringLiteral("title"), part.title},
            {QStringLiteral("questions"), part.questions},
            {QStringLiteral("answered"), part.answered},
            {QStringLiteral("mastered"), part.mastered},
            {QStringLiteral("shaky"), part.shaky},
            {QStringLiteral("accuracy"), part.accuracy()},
        });
    }
    result.insert(QStringLiteral("parts"), parts);
    return result;
}

QVariantList Trainer::combinations() const
{
    QVariantList result;
    for (const ExamCombination &combination : examCombinations()) {
        const bool available = m_catalogs.contains(combination.targetClass);
        const QVector<int> counts = m_catalogs.value(combination.targetClass).questionsPerPart;
        QStringList lines;
        int total = 0;
        for (int part : combination.parts) {
            const int questions = counts.value(part);
            total += questions;
            lines.append(tr("%1 (%2 Fragen)").arg(examPartName(part)).arg(questions));
        }
        result.append(QVariantMap{
            {QStringLiteral("id"), combination.id},
            {QStringLiteral("label"), combination.label},
            {QStringLiteral("note"), combination.note},
            {QStringLiteral("ascent"), combination.ascent},
            {QStringLiteral("available"), available},
            {QStringLiteral("parts"), lines.join(QStringLiteral(", "))},
            {QStringLiteral("total"), total},
        });
    }
    return result;
}

QDate Trainer::suggestedExamDate() const
{
    return m_examDate.isValid() ? m_examDate : QDate::currentDate().addMonths(3);
}

void Trainer::applySetup(const QString &combinationId, const QDate &examDate)
{
    const ExamCombination combination = examCombination(combinationId);
    if (combination.id.isEmpty() || !m_catalogs.contains(combination.targetClass))
        return;
    if (!openCatalog(m_catalogs.value(combination.targetClass).path))
        return;

    m_combinationId = combination.id;
    m_examDate = examDate;
    m_setupDone = true;

    QSettings settings;
    settings.setValue(QStringLiteral("startup/done"), true);
    settings.setValue(QStringLiteral("startup/combination"), combination.id);
    settings.setValue(QStringLiteral("startup/examDate"), examDate);
    QVariantList parts;
    for (int part : combination.parts)
        parts.append(part);
    settings.setValue(QStringLiteral("startup/parts"), parts);

    m_study->reset();
    m_exam->reset();
    emit setupChanged();
    emit progressChanged();
}

void Trainer::saveProgress()
{
    if (m_catalogPath.isEmpty())
        return;
    QString error;
    m_store.save(m_catalog, m_scheduler.states(), &error);
    emit progressChanged();
}

namespace {

/// Name of a Leitner stage as the statistics show it.
QString stageName(const QuestionState &state)
{
    if (state.isNew())
        return Trainer::tr("noch nie beantwortet");
    if (state.box >= Scheduler::MaxBox)
        return Trainer::tr("gefestigt");
    if (state.box <= 0)
        return Trainer::tr("wackelig");
    return Trainer::tr("Stufe %1").arg(state.box);
}

} // namespace

QVariantList Trainer::catalogParts() const
{
    QVariantList parts;
    const QSet<int> selected = effectiveParts(m_catalog, selectedParts());
    for (int i = 0; i < m_catalog.parts.size(); ++i) {
        if (!selected.contains(i))
            continue;
        const Part &part = m_catalog.parts.at(i);
        int questions = 0;
        for (const Section &section : part.sections)
            questions += section.activeQuestionCount();
        parts.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("letter"), examPartLetter(part)},
            {QStringLiteral("title"), part.title},
            {QStringLiteral("questions"), questions},
        });
    }
    return parts;
}

QVariantList Trainer::catalogSections(int part) const
{
    QVariantList sections;
    if (part < 0 || part >= m_catalog.parts.size())
        return sections;
    const QVector<Section> &list = m_catalog.parts.at(part).sections;
    for (int i = 0; i < list.size(); ++i) {
        int answered = 0;
        for (const Question *question : list.at(i).questions()) {
            if (question->isActive() && !m_scheduler.stateFor(question->id).isNew())
                ++answered;
        }
        sections.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("title"), list.at(i).title},
            {QStringLiteral("questions"), list.at(i).activeQuestionCount()},
            {QStringLiteral("answered"), answered},
        });
    }
    return sections;
}

QVariantList Trainer::catalogQuestions(int part, int section) const
{
    QVariantList questions;
    if (part < 0 || part >= m_catalog.parts.size())
        return questions;
    const QVector<Section> &sections = m_catalog.parts.at(part).sections;
    if (section < 0 || section >= sections.size())
        return questions;
    for (const Question *question : sections.at(section).questions()) {
        if (!question->isActive())
            continue;
        const QuestionState state = m_scheduler.stateFor(question->id);
        questions.append(QVariantMap{
            {QStringLiteral("id"), question->id},
            {QStringLiteral("text"), question->text.simplified()},
            {QStringLiteral("box"), state.isNew() ? -1 : state.box},
            {QStringLiteral("stage"), stageName(state)},
        });
    }
    return questions;
}

QVariantMap Trainer::questionDetail(const QString &id) const
{
    const QHash<QString, const Question *> byId = questionIndex(m_catalog);
    const Question *question = byId.value(id, nullptr);
    if (!question)
        return {};
    QVector<int> order;
    int correct = -1;
    for (int i = 0; i < question->answers.size(); ++i) {
        order.append(i);
        if (question->answers.at(i).correct)
            correct = i;
    }
    QVariantMap detail = presentQuestion(*question, order);
    const QuestionState state = m_scheduler.stateFor(id);
    detail.insert(QStringLiteral("correctPosition"), correct);
    detail.insert(QStringLiteral("stage"), stageName(state));
    detail.insert(QStringLiteral("timesAnswered"), state.timesAnswered);
    detail.insert(QStringLiteral("timesCorrect"), state.timesCorrect);
    return detail;
}

QUrl Trainer::imageUrl(const QString &relativePath) const
{
    if (relativePath.isEmpty())
        return {};
    const QString path = QDir(m_catalogDir).filePath(relativePath);
    // Unter Android liegen die Bilder in der APK; QML liest "assets:/..."
    // selbst, eine Datei-URL wuerde dort nichts finden.
    if (path.startsWith(QStringLiteral("assets:")))
        return QUrl(path);
    return QUrl::fromLocalFile(path);
}

QVariantMap Trainer::presentQuestion(const Question &question, const QVector<int> &order) const
{
    QVariantList answers;
    for (int position = 0; position < order.size(); ++position) {
        const int index = order.at(position);
        const Answer *answer = index >= 0 && index < question.answers.size()
                                   ? &question.answers.at(index)
                                   : nullptr;
        answers.append(QVariantMap{
            {QStringLiteral("letter"), QString(QChar(QLatin1Char('A').unicode() + position))},
            {QStringLiteral("text"), answer ? richText(answer->text, question.textFormat) : QString()},
            {QStringLiteral("image"), answer ? imageUrl(answer->image) : QUrl()},
        });
    }
    return QVariantMap{
        {QStringLiteral("id"), question.id},
        {QStringLiteral("text"), richText(question.text, question.textFormat)},
        {QStringLiteral("image"), imageUrl(question.image)},
        {QStringLiteral("explanation"), question.explanation.simplified()},
        {QStringLiteral("answers"), answers},
    };
}

} // namespace hrt
