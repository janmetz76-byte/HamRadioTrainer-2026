// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "core/Catalog.h"
#include "core/CatalogXml.h"
#include "exam/PaperBuilder.h"
#include "learning/Scheduler.h"

#include <QHash>
#include <QSet>
#include <QtTest>

using namespace hrt;

namespace {

/// Builds a catalog with "count" questions in one part, four answers each,
/// the first one correct, exactly like the BNetzA source layout.
Catalog makeCatalog(int count)
{
    Catalog catalog;
    catalog.metadata.title = QStringLiteral("Test");
    catalog.metadata.examClass = ExamClass::Einsteiger;
    catalog.metadata.examId = QStringLiteral("DE-E");
    catalog.metadata.validFrom = QDate(2026, 1, 1);

    Part part;
    part.id = QStringLiteral("DE-E-teil1");
    part.title = QStringLiteral("Technische Kenntnisse");
    part.order = 1;

    Section section;
    section.id = QStringLiteral("DE-E-teil1-abschnitt1");
    section.title = QStringLiteral("Abschnitt");
    section.order = 1;

    Chapter chapter;
    chapter.id = QStringLiteral("DE-E-teil1-abschnitt1-kapitel1");
    chapter.title = QStringLiteral("Kapitel");
    chapter.order = 1;

    for (int i = 1; i <= count; ++i) {
        Question question;
        question.id = QStringLiteral("Q%1").arg(i, 4, 10, QLatin1Char('0'));
        question.order = i;
        question.text = QStringLiteral("Frage %1?").arg(i);
        question.points = 2;
        question.history.addedOn = QDate(2026, 1, 1);
        for (int k = 0; k < 4; ++k) {
            Answer answer;
            answer.id = question.id + QLatin1Char('-') + QLatin1Char('A' + k);
            answer.text = QStringLiteral("Antwort %1").arg(QLatin1Char('A' + k));
            answer.correct = (k == 0);
            answer.order = k + 1;
            question.answers.append(answer);
        }
        chapter.questions.append(question);
    }

    section.chapters.append(chapter);
    part.sections.append(section);
    catalog.parts.append(part);
    return catalog;
}

} // namespace

class SchedulerTest : public QObject
{
    Q_OBJECT

private slots:
    void fullExamReproducesTheOfficialPaper();
    void trainingPaperIsNotAFullExam();
    void newQuestionsAreAskedFirst();
    void secureQuestionsAreAskedLessOften();
    void unsureCorrectAnswerStillComesBack();
    void aGuessedHitIsNotKnowledge();
    void wrongAnswerLowersTheBox();
    void paperNeverDrawsLockedOrRetiredQuestions();
    void answersAreShuffledAndNotAlwaysFirst();
    void answerOrderIsReproducible();
    void answerOrderCanBeOverridden();
    void paperRespectsPruefungsteilQuotas();
    void paperIsReproducibleForOneSeed();
};

void SchedulerTest::newQuestionsAreAskedFirst()
{
    Scheduler scheduler;
    // Learn half of the pool thoroughly.
    for (int i = 1; i <= 50; ++i) {
        scheduler.record({QStringLiteral("Q%1").arg(i, 4, 10, QLatin1Char('0')), true,
                          Confidence::Certain});
    }

    const QStringList candidates = [] {
        QStringList ids;
        for (int i = 1; i <= 100; ++i)
            ids << QStringLiteral("Q%1").arg(i, 4, 10, QLatin1Char('0'));
        return ids;
    }();

    const QVector<QString> priority = scheduler.questionsByPriority(candidates);
    QVERIFY(!priority.isEmpty());

    // The untouched half has to come before the well known half.
    const int firstKnownIndex = priority.indexOf(QStringLiteral("Q0001"));
    const int firstNewIndex = priority.indexOf(QStringLiteral("Q0060"));
    QVERIFY(firstKnownIndex >= 0);
    QVERIFY(firstNewIndex >= 0);
    QVERIFY2(firstNewIndex < firstKnownIndex,
             "eine noch nie gesehene Frage muss vor einer sicher beherrschten kommen");
}

void SchedulerTest::secureQuestionsAreAskedLessOften()
{
    Scheduler scheduler;
    AnswerOutcome certain{QStringLiteral("SECURE"), true, Confidence::Certain};
    AnswerOutcome guessing{QStringLiteral("GUESSING"), true, Confidence::Guessing};

    const QuestionState secure = scheduler.record(certain);
    const QuestionState shaky = scheduler.record(guessing);

    // The decisive property: a securely answered question is drawn less often.
    QVERIFY2(scheduler.weightFor(secure) < scheduler.weightFor(shaky),
             "eine sicher beantwortete Frage muss seltener gezogen werden");

    // A secure answer also moves the question further away (higher box), a
    // guessed correct answer barely advances it at all.
    QVERIFY(secure.box > shaky.box);

}

void SchedulerTest::unsureCorrectAnswerStillComesBack()
{
    Scheduler scheduler;
    // Answered correctly, but the learner guessed.
    scheduler.record({QStringLiteral("Q1"), true, Confidence::Guessing});

    const QuestionState state = scheduler.stateFor(QStringLiteral("Q1"));
    QCOMPARE(state.timesCorrect, 1);
    QVERIFY(state.box <= 1);
    // Despite being correct, its weight must stay far above a secure answer.
    Scheduler reference;
    reference.record({QStringLiteral("Q2"), true, Confidence::Certain});
    QVERIFY(scheduler.weightFor(state) > reference.weightFor(
                                                 reference.stateFor(QStringLiteral("Q2"))));
}

void SchedulerTest::aGuessedHitIsNotKnowledge()
{
    // Richtig geraten zaehlt in der Trefferquote, hebt die Frage aber nicht:
    // sie bleibt auf ihrer Stufe und ist morgen wieder dran. Unsicher und
    // richtig geht eine Stufe hoch, sicher und richtig drei.
    Scheduler scheduler;
    const QuestionState guessed =
        scheduler.record({QStringLiteral("G"), true, Confidence::Guessing});
    QCOMPARE(guessed.box, 0);
    QCOMPARE(guessed.timesCorrect, 1);
    QCOMPARE(guessed.consecutiveCorrect, 0);
    QCOMPARE(guessed.dueOn, QDate::currentDate().addDays(1));

    const QuestionState unsure = scheduler.record({QStringLiteral("U"), true, Confidence::Unsure});
    QCOMPARE(unsure.box, 1);
    const QuestionState certain =
        scheduler.record({QStringLiteral("C"), true, Confidence::Certain});
    QCOMPARE(certain.box, 3);

    // Auch weiter oben bleibt eine geratene Frage stehen, statt aufzusteigen.
    scheduler.record({QStringLiteral("U"), true, Confidence::Guessing});
    QCOMPARE(scheduler.stateFor(QStringLiteral("U")).box, 1);

    // Ohne Selbsteinschaetzung - aus der Pruefungssimulation - geht es wie
    // bisher eine Stufe hoch.
    const QuestionState exam = scheduler.record({QStringLiteral("P"), true, Confidence::Unknown});
    QCOMPARE(exam.box, 1);
}

void SchedulerTest::wrongAnswerLowersTheBox()
{
    Scheduler scheduler;
    for (int i = 0; i < 4; ++i)
        scheduler.record({QStringLiteral("Q1"), true, Confidence::FairlySure});
    const int highBox = scheduler.stateFor(QStringLiteral("Q1")).box;
    QVERIFY(highBox > 1);

    scheduler.record({QStringLiteral("Q1"), false, Confidence::FairlySure});
    const QuestionState after = scheduler.stateFor(QStringLiteral("Q1"));
    QCOMPARE(after.box, highBox - 2);
    QCOMPARE(after.consecutiveCorrect, 0);
    QCOMPARE(after.timesCorrect, 4);
    QCOMPARE(after.timesAnswered, 5);
}

void SchedulerTest::paperNeverDrawsLockedOrRetiredQuestions()
{
    Catalog catalog = makeCatalog(20);
    catalog.parts.first().sections.first().chapters.first().questions[0].locked = true;
    catalog.parts.first().sections.first().chapters.first().questions[1].history.retiredOn =
        QDate(2026, 6, 1);

    PaperBuilder::Options options;
    options.questionCount = 18;
    const ExamPaper paper = PaperBuilder::build(catalog, Scheduler(), options, 42);

    QCOMPARE(paper.questions.size(), 18);
    for (const PaperQuestion &entry : paper.questions) {
        QVERIFY2(entry.questionId != QStringLiteral("Q0001"), "gesperrte Frage wurde gezogen");
        QVERIFY2(entry.questionId != QStringLiteral("Q0002"), "zurückgezogene Frage wurde gezogen");
    }
}

void SchedulerTest::answersAreShuffledAndNotAlwaysFirst()
{
    Catalog catalog = makeCatalog(60);
    PaperBuilder::Options options;
    options.questionCount = 60;

    // Index the catalog by question id once.
    QHash<QString, const Question *> byId;
    for (const Question *question : catalog.questions())
        byId.insert(question->id, question);

    int correctFirst = 0;
    QSet<QString> seenOrders;
    for (quint32 seed = 1; seed <= 60; ++seed) {
        const ExamPaper paper = PaperBuilder::build(catalog, Scheduler(), options, seed);
        for (const PaperQuestion &entry : paper.questions) {
            const Question *question = byId.value(entry.questionId);
            QVERIFY(question != nullptr);
            // The source marks the first answer as correct. If the trainer
            // showed that order unchanged, the learner could always pick the
            // first entry.
            if (entry.answerOrder.first() == 0)
                ++correctFirst;
            QString pattern;
            for (int index : entry.answerOrder)
                pattern += QLatin1Char('A' + index);
            seenOrders.insert(pattern);
        }
    }
    const int distinctOrders = seenOrders.size();

    const int total = 60 * 60;
    QVERIFY2(correctFirst < total * 3 / 4,
             "die richtige Antwort darf nicht überwiegend an erster Stelle stehen");
    QVERIFY2(distinctOrders >= 20, "die Antwortreihenfolge wird nicht ausreichend gemischt");
}

void SchedulerTest::answerOrderIsReproducible()
{
    Catalog catalog = makeCatalog(10);
    PaperBuilder::Options options;
    options.questionCount = 10;

    const ExamPaper first = PaperBuilder::build(catalog, Scheduler(), options, 1234);
    const ExamPaper again = PaperBuilder::build(catalog, Scheduler(), options, 1234);

    QCOMPARE(first.seed, again.seed);
    QCOMPARE(first.questions.size(), again.questions.size());
    for (int i = 0; i < first.questions.size(); ++i) {
        QCOMPARE(first.questions.at(i).questionId, again.questions.at(i).questionId);
        QCOMPARE(first.questions.at(i).answerOrder, again.questions.at(i).answerOrder);
    }
}

void SchedulerTest::answerOrderCanBeOverridden()
{
    Catalog catalog = makeCatalog(1);
    const Question *question = catalog.questions().first();

    const QVector<int> order = PaperBuilder::parseAnswerOrder(*question, QStringLiteral("D,B,A"));
    QCOMPARE(order, (QVector<int>{3, 1, 0}));

    // The source marks answer A as correct; after an explicit order the correct
    // one must sit wherever the pattern puts it, here last.
    QCOMPARE(question->answers.at(order.last()).correct, true);

    const QVector<int> shortPattern = PaperBuilder::parseAnswerOrder(*question, QStringLiteral("B"));
    QCOMPARE(shortPattern, QVector<int>{1});
}

/// The full examination must reproduce the official exam of the
/// Amtsblatt-Verfügung 29/2024: 25 questions per Prüfungsteil, passed at 19
/// points per Prüfungsteil, six wrong answers tolerated per Prüfungsteil, and
/// 45 minutes per sheet with 60 for class A.
void SchedulerTest::fullExamReproducesTheOfficialPaper()
{
    const QString base = QStringLiteral(HRT_SOURCE_DIR "/data/catalogs");

    struct Expectation
    {
        const char *file;
        int sheets;
        int questions;    ///< total questions of the examination
        int passScore;    ///< sum of the per sheet pass marks
        int errorBudget;  ///< sum of the per sheet tolerances
        int minutes;      ///< sum of the per sheet time limits
    };
    const QVector<Expectation> expectations = {
        {"de-amateurfunk-N.xml", 3, 75, 57, 18, 135},
        {"de-amateurfunk-E.xml", 4, 100, 76, 24, 180},
        {"de-amateurfunk-A.xml", 5, 125, 95, 30, 240},
    };

    for (const Expectation &expectation : expectations) {
        Catalog catalog;
        QString error;
        const QString path = QDir(base).filePath(QString::fromLatin1(expectation.file));
        QVERIFY2(CatalogXml::load(path, &catalog, &error), qPrintable(error));

        const PaperBuilder::Options options = PaperBuilder::examOptions(catalog);
        QVERIFY(options.examMode);
        QCOMPARE(options.questionsPerPart.size(), expectation.sheets);
        for (int count : options.questionsPerPart)
            QCOMPARE(count, 25);

        const Scheduler scheduler;
        const ExamPaper paper = PaperBuilder::build(catalog, scheduler, options, 20260924u);

        QVERIFY(paper.isFullExam());
        QCOMPARE(paper.parts.size(), expectation.sheets);
        QCOMPARE(paper.questions.size(), expectation.questions);
        QCOMPARE(paper.passScore, expectation.passScore);
        QCOMPARE(paper.errorPointBudget, expectation.errorBudget);
        QCOMPARE(paper.totalMinutes, expectation.minutes);

        // Every question carries one point, so the maximum score equals the
        // question count and exactly six wrong answers are still tolerated.
        QCOMPARE(paper.totalPoints, expectation.questions);
        QCOMPARE(paper.errorPointBudget, paper.totalPoints - paper.passScore);

        // Every Prüfungsteil contributes its own share, no question is drawn
        // twice, and only real letters appear.
        const QStringList letters{QStringLiteral("V"), QStringLiteral("B"),
                                  QStringLiteral("N"), QStringLiteral("E"),
                                  QStringLiteral("A")};
        QSet<QString> drawn;
        for (int i = 0; i < paper.parts.size(); ++i) {
            const PaperPartSummary &summary = paper.parts.at(i);
            QVERIFY(letters.contains(summary.letter));
            QCOMPARE(summary.questionCount, 25);
            QCOMPARE(summary.maxPoints, 25);
            QCOMPARE(summary.passScore, 19);
            QCOMPARE(summary.errorPointBudget, 6);
            QCOMPARE(summary.minutes, summary.letter == QLatin1String("A") ? 60 : 45);
        }

        for (const PaperQuestion &entry : paper.questions) {
            QVERIFY(!drawn.contains(entry.questionId));
            drawn.insert(entry.questionId);
            QCOMPARE(entry.answerOrder.size(), 4);
            QVERIFY(entry.answerOrder.size() > 0);
        }

        // Different seeds must not produce the same paper.
        const ExamPaper other = PaperBuilder::build(catalog, scheduler, options, 1u);
        bool differs = other.questions.size() != paper.questions.size();
        for (int i = 0; !differs && i < paper.questions.size(); ++i)
            differs = other.questions.at(i).questionId != paper.questions.at(i).questionId;
        QVERIFY(differs);
    }
}

/// A training paper drawn from one Prüfungsteil must not be reported as a full
/// examination, because its limits do not apply.
void SchedulerTest::trainingPaperIsNotAFullExam()
{
    Catalog catalog = makeCatalog(40);
    PaperBuilder::Options options;
    options.questionCount = 10;

    const Scheduler scheduler;
    const ExamPaper paper = PaperBuilder::build(catalog, scheduler, options, 7u);

    QVERIFY(!paper.isFullExam());
    QVERIFY(paper.parts.isEmpty());
    QCOMPARE(paper.passScore, 0);
    QCOMPARE(paper.totalMinutes, 0);
    QCOMPARE(paper.questions.size(), 10);
}

void SchedulerTest::paperRespectsPruefungsteilQuotas()
{
    Catalog catalog = makeCatalog(30);
    // Second Prüfungsteil with its own questions.
    Part second = catalog.parts.first();
    second.id = QStringLiteral("DE-E-teil2");
    second.order = 2;
    Chapter chapter = second.sections.first().chapters.first();
    chapter.id = QStringLiteral("DE-E-teil2-abschnitt1-kapitel1");
    chapter.questions.clear();
    for (int i = 1; i <= 10; ++i) {
        Question question = catalog.parts.first().sections.first().chapters.first()
                                .questions.at(i - 1);
        question.id = QStringLiteral("T2Q%1").arg(i);
        chapter.questions.append(question);
    }
    second.sections.first().chapters = {chapter};
    catalog.parts.append(second);

    PaperBuilder::Options options;
    options.questionsPerPart = {5, 5};

    const ExamPaper paper = PaperBuilder::build(catalog, Scheduler(), options, 7);
    QCOMPARE(paper.questions.size(), 10);

    int fromSecondPart = 0;
    for (const PaperQuestion &entry : paper.questions) {
        if (entry.questionId.startsWith(QLatin1String("T2")))
            ++fromSecondPart;
    }
    QCOMPARE(fromSecondPart, 5);
}

void SchedulerTest::paperIsReproducibleForOneSeed()
{
    Catalog catalog = makeCatalog(40);
    Scheduler scheduler;
    for (int i = 1; i <= 10; ++i) {
        scheduler.record({QStringLiteral("Q%1").arg(i, 4, 10, QLatin1Char('0')), i % 2 == 0,
                          i % 3 == 0 ? Confidence::Unsure : Confidence::Certain});
    }

    PaperBuilder::Options options;
    options.questionCount = 12;

    const ExamPaper a = PaperBuilder::build(catalog, scheduler, options, 99);
    const ExamPaper b = PaperBuilder::build(catalog, scheduler, options, 99);
    QCOMPARE(a.questions.size(), 12);
    QCOMPARE(a.questions.size(), b.questions.size());
    for (int i = 0; i < a.questions.size(); ++i)
        QCOMPARE(a.questions.at(i).questionId, b.questions.at(i).questionId);

    // A different seed must normally give a different paper.
    const ExamPaper c = PaperBuilder::build(catalog, scheduler, options, 100);
    bool differs = a.questions.size() != c.questions.size();
    for (int i = 0; !differs && i < a.questions.size(); ++i)
        differs = a.questions.at(i).questionId != c.questions.at(i).questionId;
    QVERIFY(differs);
}

QTEST_MAIN(SchedulerTest)
#include "tst_scheduler.moc"