// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "learning/LearningStatistics.h"
#include "learning/StudySession.h"

#include <QtTest>

#include <numeric>

using namespace hrt;

namespace {

constexpr char kDate[] = "2026-09-24";

/// A catalog of one Prüfungsteil with the given number of four-answer questions.
Catalog makeCatalog(int questions, const QString &letter = QStringLiteral("N"))
{
    Catalog catalog;
    catalog.metadata.examId = QStringLiteral("DE-") + letter;
    catalog.metadata.examClass = ExamClass::Novize;

    Part part;
    part.id = QStringLiteral("DE-%1-teil1-%2").arg(letter, letter.toLower());
    part.title = QStringLiteral("Technische Kenntnisse, Klasse %1").arg(letter);
    part.order = 1;

    Section section;
    section.id = part.id + QStringLiteral("-abschnitt1");
    section.title = QStringLiteral("Abschnitt");
    section.order = 1;

    Chapter chapter;
    chapter.id = section.id + QStringLiteral("-kapitel1");
    chapter.title = QStringLiteral("Kapitel");
    chapter.order = 1;

    for (int i = 1; i <= questions; ++i) {
        Question question;
        question.id = QStringLiteral("Q%1").arg(i, 4, 10, QLatin1Char('0'));
        question.order = i;
        question.text = QStringLiteral("Frage %1?").arg(i);
        for (int k = 0; k < 4; ++k) {
            Answer answer;
            answer.id = question.id + QLatin1Char('A' + k);
            answer.text = QStringLiteral("Antwort %1").arg(QLatin1Char('A' + k));
            answer.correct = (k == 0);
            question.answers.append(answer);
        }
        chapter.questions.append(question);
    }
    section.chapters.append(chapter);
    part.sections.append(section);
    catalog.parts.append(part);
    return catalog;
}

/// Answers one question with the given confidence, so the test can place a
/// question in a defined Leitner box.
void answer(Scheduler *scheduler, const QString &id, bool correct, Confidence confidence)
{
    AnswerOutcome outcome;
    outcome.questionId = id;
    outcome.correct = correct;
    outcome.confidence = confidence;
    scheduler->record(outcome);
}

} // namespace

class LearningTest : public QObject
{
    Q_OBJECT

private slots:
    void freshCatalogHasNothingAnswered();
    void answersAreCountedAndBucketed();
    void shakyAndMasteredAreDistinguished();
    void dueDependsOnTheLeitnerSpacing();
    void statisticsAreReportedPerPart();
    void lockedAndRetiredQuestionsAreIgnored();
    void lockedAndRetiredQuestionsAreIgnored_data();

    void planPutsShakyQuestionsFirst();
    void planCanBeLimited();
    void planCanBeRestrictedToDueQuestions();
    void planCanBeRestrictedToOnePart();
    void planCanBeRestrictedToShakyQuestions();
    void freshQuestionsCanBeLeftOut();

    void confidenceRaisesTheBoxFaster();
    void guessedButCorrectStaysCloseToTheTop();
    void objectiveResultSurvivesAMissingSelfReport();
    void confirmedConfidenceIsKeptAcrossRounds();
};

void LearningTest::freshCatalogHasNothingAnswered()
{
    const Catalog catalog = makeCatalog(20);
    const Scheduler scheduler;
    const LearningStatistics statistics = computeStatistics(catalog, scheduler, QDate::fromString(kDate, Qt::ISODate));

    QCOMPARE(statistics.questions, 20);
    QCOMPARE(statistics.answered, 0);
    QCOMPARE(statistics.fresh, 20);
    QCOMPARE(statistics.shaky, 0);
    QCOMPARE(statistics.mastered, 0);
    QCOMPARE(statistics.due, 0);
    QCOMPARE(statistics.timesAnswered, 0);
    QCOMPARE(statistics.accuracy(), 0.0);
    QCOMPARE(statistics.coverage(), 0.0);
    QCOMPARE(statistics.priorityCount(), 20);
    QCOMPARE(statistics.byBox.size(), Scheduler::MaxBox + 1);
}

void LearningTest::answersAreCountedAndBucketed()
{
    Catalog catalog = makeCatalog(20);
    Scheduler scheduler;

    for (int i = 1; i <= 10; ++i)
        answer(&scheduler, QStringLiteral("Q%1").arg(i, 4, 10, QLatin1Char('0')), true,
               Confidence::Certain);

    const LearningStatistics statistics = computeStatistics(catalog, scheduler, QDate::fromString(kDate, Qt::ISODate));
    QCOMPARE(statistics.questions, 20);
    QCOMPARE(statistics.answered, 10);
    QCOMPARE(statistics.fresh, 10);
    QCOMPARE(statistics.timesAnswered, 10);
    QCOMPARE(statistics.timesCorrect, 10);
    QCOMPARE(statistics.accuracy(), 100.0);
    QCOMPARE(statistics.coverage(), 50.0);
    QCOMPARE(std::accumulate(statistics.byBox.cbegin(), statistics.byBox.cend(), 0), 10);
}

void LearningTest::shakyAndMasteredAreDistinguished()
{
    Catalog catalog = makeCatalog(20);
    Scheduler scheduler;

    // Wrong answers keep a question in box 0.
    for (int i = 1; i <= 3; ++i)
        answer(&scheduler, QStringLiteral("Q%1").arg(i, 4, 10, QLatin1Char('0')), false,
               Confidence::Unsure);
    // Certain and correct twice moves a question far up.
    for (int i = 4; i <= 5; ++i)
        for (int k = 0; k < 3; ++k)
            answer(&scheduler, QStringLiteral("Q%1").arg(i, 4, 10, QLatin1Char('0')), true,
                   Confidence::Certain);

    const LearningStatistics statistics = computeStatistics(catalog, scheduler, QDate::fromString(kDate, Qt::ISODate));
    QCOMPARE(statistics.shaky, 3);
    QVERIFY(statistics.mastered >= 2);
    QCOMPARE(statistics.accuracy(), (0.0 + 6.0) / 9.0 * 100.0);
}

void LearningTest::dueDependsOnTheLeitnerSpacing()
{
    Catalog catalog = makeCatalog(20);
    Scheduler scheduler;

    // Answered wrong today: due again immediately.
    answer(&scheduler, QStringLiteral("Q0001"), false, Confidence::Unsure);
    // Answered correctly with certainty once: box 3, so due in seven days.
    // A second certain answer would already reach the top box, which is never
    // due again.
    answer(&scheduler, QStringLiteral("Q0002"), true, Confidence::Certain);
    QCOMPARE(scheduler.stateFor(QStringLiteral("Q0002")).box, 3);

    // The scheduler files the next date relative to the real today, so the
    // comparison has to use the same day.
    const QDate today = QDate::currentDate();
    const LearningStatistics now = computeStatistics(catalog, scheduler, today);
    QCOMPARE(now.byBox.at(0), 1);
    QCOMPARE(now.due, 1); // only the wrong one

    // In seven days the correct one comes back as well.
    const LearningStatistics later = computeStatistics(catalog, scheduler, today.addDays(7));
    QCOMPARE(later.due, 2);

    // A question that is not mastered never stops being due: in the Leitner
    // model the spacing only postpones it, it does not retire it.
    const LearningStatistics muchLater =
        computeStatistics(catalog, scheduler, today.addDays(400));
    QCOMPARE(muchLater.due, 2);

    // Only the top box leaves the rotation. Questions that were never seen
    // have their own figure and are not counted as a repetition.
    Scheduler secure;
    for (int k = 0; k < 3; ++k)
        answer(&secure, QStringLiteral("Q0002"), true, Confidence::Certain);
    QCOMPARE(secure.stateFor(QStringLiteral("Q0002")).box, Scheduler::MaxBox);

    const LearningStatistics settled = computeStatistics(catalog, secure, today);
    QCOMPARE(settled.due, 0);
    QCOMPARE(settled.mastered, 1);
    QCOMPARE(settled.fresh, 19);
}

void LearningTest::statisticsAreReportedPerPart()
{
    Catalog catalog = makeCatalog(10, QStringLiteral("N"));
    // Add a second Prüfungsteil, as in a real catalog of class E.
    Part second = catalog.parts.first();
    second.id = QStringLiteral("DE-E-teil2-e");
    second.title = QStringLiteral("Technische Kenntnisse, Klasse E");
    second.order = 2;
    // The copy still holds the questions of the first part.
    second.sections.first().chapters.first().questions.clear();
    for (const Question &source : catalog.parts.first().sections.first()
                                      .chapters.first()
                                      .questions) {
        Question copy = source;
        copy.id = source.id + QStringLiteral("E");
        second.sections.first().chapters.first().questions.append(copy);
    }
    catalog.parts.append(second);

    Scheduler scheduler;
    answer(&scheduler, QStringLiteral("Q0001"), true, Confidence::FairlySure);
    answer(&scheduler, QStringLiteral("Q0001E"), false, Confidence::Guessing);

    const LearningStatistics statistics = computeStatistics(catalog, scheduler, QDate::fromString(kDate, Qt::ISODate));
    QCOMPARE(statistics.parts.size(), 2);
    QCOMPARE(statistics.questions, 20);

    const PartProgress n = statistics.parts.at(0);
    QCOMPARE(n.letter, QStringLiteral("N"));
    QCOMPARE(n.questions, 10);
    QCOMPARE(n.answered, 1);
    QCOMPARE(n.accuracy(), 100.0);

    const PartProgress e = statistics.parts.at(1);
    QCOMPARE(e.letter, QStringLiteral("E"));
    QCOMPARE(e.questions, 10);
    QCOMPARE(e.answered, 1);
    QCOMPARE(e.shaky, 1);
    QCOMPARE(e.accuracy(), 0.0);
}

void LearningTest::lockedAndRetiredQuestionsAreIgnored_data()
{
    QTest::addColumn<bool>("locked");
    QTest::addColumn<bool>("retired");
    QTest::newRow("gesperrt") << true << false;
    QTest::newRow("zurückgezogen") << false << true;
    QTest::newRow("beides") << true << true;
}

void LearningTest::lockedAndRetiredQuestionsAreIgnored()
{
    QFETCH(bool, locked);
    QFETCH(bool, retired);

    Catalog catalog = makeCatalog(10);
    Question &question = catalog.parts.first().sections.first().chapters.first().questions.first();
    question.locked = locked;
    if (retired)
        question.history.retiredOn = QDate(2025, 1, 1);

    const QString removedId = question.id;

    const Scheduler scheduler;
    const LearningStatistics statistics =
        computeStatistics(catalog, scheduler, QDate::fromString(kDate, Qt::ISODate));
    QCOMPARE(statistics.questions, 9);
    QCOMPARE(statistics.fresh, 9);

    // They must not be drawn into a practice round either.
    StudySession session(catalog, const_cast<Scheduler *>(&scheduler), {});
    QCOMPARE(session.count(), 9);
    QVERIFY(!session.plan().contains(removedId));
}

void LearningTest::planPutsShakyQuestionsFirst()
{
    Catalog catalog = makeCatalog(10);
    Scheduler scheduler;

    // A question the learner keeps failing.
    for (int k = 0; k < 4; ++k)
        answer(&scheduler, QStringLiteral("Q0001"), false, Confidence::Unsure);
    // A question that is secure.
    for (int k = 0; k < 3; ++k)
        answer(&scheduler, QStringLiteral("Q0002"), true, Confidence::Certain);

    StudySession session(catalog, &scheduler, {});
    QCOMPARE(session.count(), 10);

    // Unseen questions come first, so that the whole pool is covered once.
    QCOMPARE(session.freshCount(), 8);
    for (int i = 0; i < 8; ++i)
        QCOMPARE(session.plan().at(i), QStringLiteral("Q%1").arg(i + 3, 4, 10, QLatin1Char('0')));

    // After that the shaky question has to come before the secure one.
    QCOMPARE(session.plan().at(8), QStringLiteral("Q0001"));
    QCOMPARE(session.plan().at(9), QStringLiteral("Q0002"));

    // Every planned question belongs to the catalog, and none appears twice.
    QSet<QString> planned(session.plan().cbegin(), session.plan().cend());
    QCOMPARE(planned.size(), session.count());
    QCOMPARE(session.partLetterOf(QStringLiteral("Q0001")), QStringLiteral("N"));
}

void LearningTest::planCanBeLimited()
{
    Catalog catalog = makeCatalog(50);
    Scheduler scheduler;

    StudyOptions options;
    options.maxQuestions = 7;
    StudySession session(catalog, &scheduler, options);
    QCOMPARE(session.count(), 7);
    QVERIFY(session.freshCount() == 7);

    // 0 means no limit.
    options.maxQuestions = 0;
    QCOMPARE(StudySession(catalog, &scheduler, options).count(), 50);
}

void LearningTest::planCanBeRestrictedToDueQuestions()
{
    Catalog catalog = makeCatalog(10);
    Scheduler scheduler;

    // Wrong today: due immediately.
    answer(&scheduler, QStringLiteral("Q0001"), false, Confidence::Unsure);
    // Correct with certainty once: box 3, due in seven days.
    answer(&scheduler, QStringLiteral("Q0002"), true, Confidence::Certain);

    StudyOptions options;
    options.onlyDue = true;
    StudySession session(catalog, &scheduler, options);

    // The new ones count as due, because they were never seen, but the
    // repetition that is actually due has to come first.
    QCOMPARE(session.count(), 9); // eight fresh plus the shaky one
    QCOMPARE(session.plan().first(), QStringLiteral("Q0001"));
    QVERIFY(!session.plan().contains(QStringLiteral("Q0002")));

    // Without the new ones, only the shaky one is left.
    options.includeFresh = false;
    StudySession onlyShakyDue(catalog, &scheduler, options);
    QCOMPARE(onlyShakyDue.plan(), QStringList{QStringLiteral("Q0001")});

    // Without the filter the pool comes first, because the whole point of a
    // round is to see every question once.
    StudySession unfiltered(catalog, &scheduler, {});
    QVERIFY(!unfiltered.plan().first().isEmpty());
    QCOMPARE(unfiltered.plan().first(), QStringLiteral("Q0003"));
}

void LearningTest::planCanBeRestrictedToOnePart()
{
    Catalog catalog = makeCatalog(10, QStringLiteral("N"));
    Part second = catalog.parts.first();
    second.id = QStringLiteral("DE-E-teil2-e");
    second.title = QStringLiteral("Technische Kenntnisse, Klasse E");
    second.order = 2;
    // The copy still holds the questions of the first part.
    second.sections.first().chapters.first().questions.clear();
    for (const Question &source : catalog.parts.first().sections.first()
                                      .chapters.first()
                                      .questions) {
        Question copy = source;
        copy.id = source.id + QStringLiteral("E");
        second.sections.first().chapters.first().questions.append(copy);
    }
    catalog.parts.append(second);

    Scheduler scheduler;
    StudyOptions options;
    options.parts = {QStringLiteral("E")};
    StudySession session(catalog, &scheduler, options);

    QCOMPARE(session.count(), 10);
    for (const QString &id : session.plan()) {
        QVERIFY(id.endsWith(QLatin1Char('E')));
        QCOMPARE(session.partLetterOf(id), QStringLiteral("E"));
    }
}

void LearningTest::planCanBeRestrictedToShakyQuestions()
{
    Catalog catalog = makeCatalog(10);
    Scheduler scheduler;

    answer(&scheduler, QStringLiteral("Q0001"), false, Confidence::Unsure);
    answer(&scheduler, QStringLiteral("Q0002"), true, Confidence::FairlySure);

    StudyOptions options;
    options.onlyShaky = true;
    StudySession session(catalog, &scheduler, options);

    QCOMPARE(session.plan(), QStringList{QStringLiteral("Q0001")});
}

/// This is the whole point of the trainer: a wrong answer that the learner
/// doubted has to come back sooner than one that was known.
/// Sweeping the whole catalog first is often not what the learner wants on a
/// short practice round.
void LearningTest::freshQuestionsCanBeLeftOut()
{
    Catalog catalog = makeCatalog(10);
    Scheduler scheduler;
    answer(&scheduler, QStringLiteral("Q0001"), false, Confidence::Unsure);
    answer(&scheduler, QStringLiteral("Q0002"), true, Confidence::FairlySure);

    StudyOptions options;
    options.includeFresh = false;
    StudySession session(catalog, &scheduler, options);

    QCOMPARE(session.count(), 2);
    QVERIFY(session.freshCount() == 0);
    QVERIFY(session.plan().contains(QStringLiteral("Q0001")));
    QVERIFY(session.plan().contains(QStringLiteral("Q0002")));
}

void LearningTest::confidenceRaisesTheBoxFaster()
{
    Catalog catalog = makeCatalog(10);
    Scheduler certain;
    Scheduler guessed;
    for (int k = 0; k < 2; ++k) {
        answer(&certain, QStringLiteral("Q0001"), true, Confidence::Certain);
        answer(&guessed, QStringLiteral("Q0001"), true, Confidence::Guessing);
    }

    QVERIFY(certain.stateFor(QStringLiteral("Q0001")).box
            > guessed.stateFor(QStringLiteral("Q0001")).box);
    QVERIFY(guessed.stateFor(QStringLiteral("Q0001")).dueOn
            < certain.stateFor(QStringLiteral("Q0001")).dueOn);

    // A correct answer while guessing stays near the front of the queue, a
    // certain one sinks.
    StudySession shaky(catalog, &guessed, {});
    StudySession secure(catalog, &certain, {});
    QVERIFY(guessed.weightFor(guessed.stateFor(QStringLiteral("Q0001")))
            > certain.weightFor(certain.stateFor(QStringLiteral("Q0001"))));
}

/// Guessed but correct: the box still rises, but not as far as when certain.
void LearningTest::guessedButCorrectStaysCloseToTheTop()
{
    Scheduler scheduler;
    answer(&scheduler, QStringLiteral("Q0001"), true, Confidence::Guessing);
    const QuestionState state = scheduler.stateFor(QStringLiteral("Q0001"));
    // Der Treffer zaehlt, gilt aber nicht als Wissen: keine Stufe hoeher,
    // keine Serie, und morgen ist die Frage wieder dran.
    QCOMPARE(state.timesCorrect, 1);
    QCOMPARE(state.consecutiveCorrect, 0);
    QCOMPARE(state.box, 0);
    QCOMPARE(state.dueOn, QDate::currentDate().addDays(1));
    QCOMPARE(state.confidence, Confidence::Guessing);
}

/// An answer without a self report must still move the question, otherwise the
/// learner would get credit for knowing nothing about how sure they were.
void LearningTest::objectiveResultSurvivesAMissingSelfReport()
{
    Catalog catalog = makeCatalog(10);
    Scheduler scheduler;
    StudySession session(catalog, &scheduler, {});

    session.record(QStringLiteral("Q0001"), false, Confidence::Unknown);

    const QuestionState state = scheduler.stateFor(QStringLiteral("Q0001"));
    QCOMPARE(state.timesAnswered, 1);
    QCOMPARE(state.timesCorrect, 0);
    QCOMPARE(state.consecutiveCorrect, 0);
    QCOMPARE(state.confidence, Confidence::Unknown);
}

/// A self report from an earlier round must not be erased by a later round
/// that does not ask again.
void LearningTest::confirmedConfidenceIsKeptAcrossRounds()
{
    Catalog catalog = makeCatalog(10);
    Scheduler scheduler;
    StudySession session(catalog, &scheduler, {});

    session.record(QStringLiteral("Q0001"), true, Confidence::FairlySure);
    QCOMPARE(scheduler.stateFor(QStringLiteral("Q0001")).confidence, Confidence::FairlySure);

    session.record(QStringLiteral("Q0001"), true, Confidence::Unknown);
    const QuestionState state = scheduler.stateFor(QStringLiteral("Q0001"));
    QCOMPARE(state.timesAnswered, 2);
    QCOMPARE(state.timesCorrect, 2);
    QCOMPARE(state.confidence, Confidence::FairlySure);
}

QTEST_MAIN(LearningTest)
#include "tst_learning.moc"