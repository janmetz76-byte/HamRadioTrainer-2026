// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "PaperBuilder.h"

#include "core/Catalog.h"

#include <algorithm>
#include <numeric>

namespace hrt {
namespace {

/// Deterministic per question seed: same paper seed and same question id always
/// produce the same answer order, so a paper can be reviewed or reprinted.
quint64 questionSeed(quint32 paperSeed, const QString &questionId)
{
    quint64 hash = 1469598103934665603ULL; // FNV-1a offset basis
    const quint32 seedBytes = paperSeed;
    for (int i = 0; i < 4; ++i) {
        hash ^= (seedBytes >> (8 * i)) & 0xff;
        hash *= 1099511628211ULL;
    }
    for (const QChar character : questionId) {
        hash ^= character.unicode() & 0xff;
        hash *= 1099511628211ULL;
        hash ^= (character.unicode() >> 8) & 0xff;
        hash *= 1099511628211ULL;
    }
    return hash;
}

/// Weighted draw without replacement.
QVector<const Question *> drawWeighted(const QVector<const Question *> &pool,
                                       const QVector<double> &weights, int count,
                                       quint32 seed)
{
    QRandomGenerator generator(seed);
    std::discrete_distribution<int> distribution(weights.cbegin(), weights.cend());

    QVector<bool> taken(pool.size(), false);
    QVector<const Question *> picked;
    picked.reserve(std::min<qsizetype>(count, pool.size()));

    // Guard against an exhausted pool and against zero weights.
    int attempts = 0;
    const int maxAttempts = (count + 1) * 32 + 64;
    while (picked.size() < count && attempts < maxAttempts) {
        ++attempts;
        const int index = distribution(generator);
        if (index < 0 || index >= pool.size() || taken.at(index))
            continue;
        taken[index] = true;
        picked.append(pool.at(index));
    }

    // Fall back to plain order for whatever the weighted draw did not produce.
    if (picked.size() < count) {
        for (int i = 0; i < pool.size() && picked.size() < count; ++i) {
            if (!taken.at(i)) {
                taken[i] = true;
                picked.append(pool.at(i));
            }
        }
    }

    return picked;
}

/// A drawn question together with the Prüfungsteil it came from.
struct Pick
{
    const Question *question = nullptr;
    int partIndex = -1;
};

} // namespace

bool ExamPaper::isFullExam() const
{
    if (parts.isEmpty())
        return false;
    for (const PaperPartSummary &part : parts)
        if (part.questionCount <= 0 || part.passScore <= 0)
            return false;
    return passScore > 0 && totalMinutes > 0;
}

QString PaperBuilder::partLetter(const Part &part)
{
    return examPartLetter(part);
}

PaperBuilder::Options PaperBuilder::examOptions(const Catalog &catalog)
{
    Options options;
    options.examMode = true;
    for (const Part &part : catalog.parts)
        options.questionsPerPart.append(part.selection.questions);
    return options;
}

QVector<int> PaperBuilder::shuffledAnswerOrder(const Question &question, quint32 seed,
                                              const QString &questionId)
{
    QVector<int> order(question.answers.size());
    std::iota(order.begin(), order.end(), 0);

    QRandomGenerator generator(static_cast<quint32>(questionSeed(seed, questionId)));
    for (int i = order.size() - 1; i > 0; --i) {
        order.swapItemsAt(i, static_cast<int>(generator.bounded(i + 1)));
    }
    return order;
}

QVector<int> PaperBuilder::parseAnswerOrder(const Question &question, const QString &pattern)
{
    QVector<int> order;
    const QStringList letters = pattern.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &letter : letters) {
        const QString trimmed = letter.trimmed().toUpper();
        if (trimmed.isEmpty())
            continue;
        const int index = trimmed.at(0).unicode() - QLatin1Char('A').unicode();
        if (index >= 0 && index < question.answers.size())
            order.append(index);
    }
    // Keep only the first occurrence of every index.
    order.erase(std::unique(order.begin(), order.end()), order.end());
    return order;
}

ExamPaper PaperBuilder::build(const Catalog &catalog, const Scheduler &scheduler,
                              const Options &options, quint32 seed)
{
    ExamPaper paper;
    paper.seed = seed;

    QVector<Pick> picks;

    // Collect the drawable questions of one Prüfungsteil together with their
    // weights. Locked and retired questions never enter the pool.
    const auto poolForPart = [&scheduler, &options](const Part &part, QVector<double> *weights) {
        QVector<const Question *> pool;
        for (const Section &section : part.sections) {
            for (const Question *question : section.questions()) {
                if (!question->isActive())
                    continue;
                pool.append(question);
                // Die Pruefungssimulation zieht wie die Bundesnetzagentur
                // gleichmaessig aus dem ganzen Katalog. Nach dem Lernstand
                // gewichtet kamen gelernte Fragen kaum noch vor, und das
                // Ergebnis sagte wenig ueber die echte Pruefung.
                weights->append(options.reviewMode || options.examMode
                                    ? 1.0
                                    : scheduler.weightFor(scheduler.stateFor(question->id)));
            }
        }
        return pool;
    };

    const auto collect = [&poolForPart](const QVector<const Question *> &drawn, int partIndex) {
        QVector<Pick> result;
        result.reserve(drawn.size());
        for (const Question *question : drawn)
            result.append(Pick{question, partIndex});
        return result;
    };

    const bool perPart = options.examMode || !options.questionsPerPart.isEmpty();
    if (perPart) {
        for (int partIndex = 0;
             partIndex < catalog.parts.size() && partIndex < options.questionsPerPart.size();
             ++partIndex) {
            const int wanted = options.questionsPerPart.at(partIndex);
            if (wanted <= 0)
                continue;
            QVector<double> weights;
            const QVector<const Question *> pool = poolForPart(catalog.parts.at(partIndex), &weights);
            picks.append(collect(drawWeighted(pool, weights, wanted, seed + partIndex), partIndex));
        }
    } else {
        QVector<const Question *> pool;
        QVector<double> weights;
        for (int partIndex = 0; partIndex < catalog.parts.size(); ++partIndex) {
            QVector<double> partWeights;
            const QVector<const Question *> partPool = poolForPart(catalog.parts.at(partIndex),
                                                                   &partWeights);
            pool.append(partPool);
            weights.append(partWeights);
        }
        picks = collect(drawWeighted(pool, weights, options.questionCount, seed), -1);
    }

    // Prüfungsteil limits, in the order of the catalog.
    QVector<int> partIndexOfQuestion(picks.size(), -1);
    if (options.examMode) {
        for (int partIndex = 0; partIndex < catalog.parts.size(); ++partIndex) {
            const Part &part = catalog.parts.at(partIndex);
            if (options.questionsPerPart.value(partIndex) <= 0)
                continue;
            PaperPartSummary summary;
            summary.partId = part.id;
            summary.letter = partLetter(part);
            summary.title = part.title;
            summary.passScore = part.selection.passScore.value_or(0);
            summary.errorPointBudget = part.selection.errorPoints.value_or(0);
            summary.minutes = part.selection.minutes.value_or(0);
            paper.parts.append(summary);
        }
    }

    for (int i = 0; i < picks.size(); ++i) {
        const Pick &pick = picks.at(i);
        PaperQuestion entry;
        entry.questionId = pick.question->id;
        entry.answerOrder = shuffledAnswerOrder(*pick.question, seed, pick.question->id);
        if (pick.partIndex >= 0) {
            const Part &part = catalog.parts.at(pick.partIndex);
            entry.partId = part.id;
            entry.partLetter = partLetter(part);
        }
        paper.questions.append(entry);

        const int points = pick.question->effectivePoints(catalog.metadata);
        paper.totalPoints += points;

        if (pick.partIndex >= 0) {
            partIndexOfQuestion[i] = pick.partIndex;
            for (PaperPartSummary &summary : paper.parts) {
                if (summary.partId == catalog.parts.at(pick.partIndex).id) {
                    summary.questionCount += 1;
                    summary.maxPoints += points;
                    break;
                }
            }
        }
    }

    if (options.examMode) {
        for (const PaperPartSummary &summary : paper.parts) {
            paper.passScore += summary.passScore;
            paper.errorPointBudget += summary.errorPointBudget;
            paper.totalMinutes += summary.minutes;
        }
    }

    return paper;
}

} // namespace hrt