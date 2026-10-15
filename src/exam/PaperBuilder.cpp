// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "PaperBuilder.h"

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

} // namespace

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

    QVector<const Question *> picked;

    const bool perPart = !options.questionsPerPart.isEmpty();
    if (perPart) {
        for (int partIndex = 0;
             partIndex < catalog.parts.size() && partIndex < options.questionsPerPart.size();
             ++partIndex) {
            const int wanted = options.questionsPerPart.at(partIndex);
            if (wanted <= 0)
                continue;
            const Part &part = catalog.parts.at(partIndex);

            QVector<const Question *> pool;
            QVector<double> weights;
            for (const Section &section : part.sections) {
                for (const Question *question : section.questions()) {
                    if (!question->isActive())
                        continue;
                    pool.append(question);
                    weights.append(options.reviewMode ? 1.0
                                                      : scheduler.weightFor(
                                                            scheduler.stateFor(question->id)));
                }
            }
            picked.append(drawWeighted(pool, weights, wanted, seed + partIndex));
        }
    } else {
        QVector<const Question *> pool;
        QVector<double> weights;
        for (const Part &part : catalog.parts) {
            for (const Section &section : part.sections) {
                for (const Question *question : section.questions()) {
                    if (!question->isActive())
                        continue;
                    pool.append(question);
                    weights.append(options.reviewMode
                                       ? 1.0
                                       : scheduler.weightFor(scheduler.stateFor(question->id)));
                }
            }
        }
        picked = drawWeighted(pool, weights, options.questionCount, seed);
    }

    for (const Question *question : std::as_const(picked)) {
        PaperQuestion entry;
        entry.questionId = question->id;
        entry.answerOrder = shuffledAnswerOrder(*question, seed, question->id);
        paper.questions.append(entry);
        paper.totalPoints += question->effectivePoints(catalog.metadata);
    }

    return paper;
}

} // namespace hrt