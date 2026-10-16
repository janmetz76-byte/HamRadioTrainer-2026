// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "core/CatalogXml.h"
#include "exam/ExamSession.h"
#include "learning/Scheduler.h"

#include <QtTest>

using namespace hrt;

namespace {

Catalog loadClassCatalog(const char *file)
{
    Catalog catalog;
    QString error;
    const QString path = QDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"))
                             .filePath(QString::fromLatin1(file));
    if (!CatalogXml::load(path, &catalog, &error))
        qFatal("Katalog %s nicht lesbar: %s", file, qPrintable(error));
    return catalog;
}

ExamSession fullExam(const Catalog &catalog, quint32 seed)
{
    const Scheduler scheduler;
    const PaperBuilder::Options options = PaperBuilder::examOptions(catalog);
    return ExamSession(catalog, PaperBuilder::build(catalog, scheduler, options, seed));
}

/// Answers every question either correctly or, with wrongAnswers in {1}, wrongly.
void answerAll(ExamSession *session, const QSet<int> &wrongIndexes)
{
    for (int i = 0; i < session->count(); ++i) {
        const int correct = session->correctPositionAt(i);
        QVERIFY(correct >= 0);
        int chosen = correct;
        if (wrongIndexes.contains(i)) {
            chosen = (correct + 1) % session->answerOrderAt(i).size();
        }
        session->answer(i, chosen);
    }
}

} // namespace

class ExamSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void aCompletePaperIsAnsweredCorrectly();
    void unansweredQuestionsCountAsWrong();
    void oneFailedPartFailsTheWholeExamination();
    void passMarkAppliesPerPartNotInTotal();
    void nextUnansweredVisitsEveryQuestionOnce();
    void invalidAnswerPositionsAreIgnored();
    void trainingPaperHasNoPassMarks();
    void correctAnswerIsNotAlwaysFirst();
};

/// A fully correct class E paper: 19 of 25 points in every one of the four
/// Prüfungsteile, so the certificate is awarded.
void ExamSessionTest::aCompletePaperIsAnsweredCorrectly()
{
    const Catalog catalog = loadClassCatalog("de-amateurfunk-E.xml");
    ExamSession session = fullExam(catalog, 20260924u);

    QCOMPARE(session.count(), 100);
    QCOMPARE(session.answeredCount(), 0);
    QVERIFY(session.partLetterAt(0).isEmpty() == false);

    answerAll(&session, {});
    QCOMPARE(session.answeredCount(), 100);

    const ExamResult result = session.evaluate();
    QVERIFY(result.passed);
    QCOMPARE(result.parts.size(), 4);
    QCOMPARE(result.achievedPoints, 100);
    QCOMPARE(result.maxPoints, 100);
    QCOMPARE(result.passScore, 76);
    QCOMPARE(result.wrongAnswers, 0);
    QCOMPARE(result.unanswered, 0);
    QVERIFY(result.failedParts().isEmpty());

    for (const PartResult &part : result.parts) {
        QCOMPARE(part.questionCount, 25);
        QCOMPARE(part.achievedPoints, 25);
        QCOMPARE(part.passScore, 19);
        QCOMPARE(part.errorPointBudget, 6);
        QVERIFY(part.passed());
        QVERIFY(part.failedQuestionIds.isEmpty());
    }

    const QStringList letters{QStringLiteral("V"), QStringLiteral("B"), QStringLiteral("N"),
                              QStringLiteral("E")};
    for (const QString &letter : letters)
        QCOMPARE(session.countOfPart(letter), 25);
}

/// A question left blank is wrong, because the Verfügung grants points only
/// for correct answers.
void ExamSessionTest::unansweredQuestionsCountAsWrong()
{
    const Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    ExamSession session = fullExam(catalog, 1u);
    QCOMPARE(session.count(), 75);

    // Part V is the first 25 questions of the paper.
    for (int i = 25; i < 75; ++i)
        session.answer(i, session.correctPositionAt(i));
    QCOMPARE(session.answeredCount(), 50);
    QCOMPARE(session.answeredCountOfPart(QStringLiteral("V")), 0);
    QCOMPARE(session.answeredCountOfPart(QStringLiteral("N")), 25);

    const ExamResult result = session.evaluate();
    QCOMPARE(result.unanswered, 25);
    QCOMPARE(result.wrongAnswers, 25);
    QCOMPARE(result.achievedPoints, 50);

    const PartResult &partV = result.parts.first();
    QCOMPARE(partV.letter, QStringLiteral("V"));
    QCOMPARE(partV.questionCount, 25);
    QCOMPARE(partV.achievedPoints, 0);
    QCOMPARE(partV.unanswered, 25);
    QCOMPARE(partV.failedQuestionIds.size(), 25);
    QVERIFY(!partV.passed());
}

/// Only a single failing Prüfungsteil blocks the certificate, and that is the
/// part that has to be repeated orally.
void ExamSessionTest::oneFailedPartFailsTheWholeExamination()
{
    const Catalog catalog = loadClassCatalog("de-amateurfunk-E.xml");
    ExamSession session = fullExam(catalog, 99u);

    // Six wrong answers in Prüfungsteil N reach exactly its tolerance, seven
    // are one too many. Part N is the third Prüfungsteil, questions 50..74.
    QSet<int> wrong;
    for (int i = 0; i < 7; ++i)
        wrong.insert(50 + i);
    answerAll(&session, wrong);

    const ExamResult result = session.evaluate();
    QVERIFY(!result.passed);
    QCOMPARE(result.failedParts(), QStringList{QStringLiteral("N")});
    QCOMPARE(result.weakestShortfall(), 1);

    const PartResult &partN = result.parts.at(2);
    QCOMPARE(partN.letter, QStringLiteral("N"));
    QCOMPARE(partN.achievedPoints, 18);
    QCOMPARE(partN.passScore, 19);
    QCOMPARE(partN.errorPointBudget, 6);
    QCOMPARE(partN.wrongAnswers, 7);
    QVERIFY(!partN.passed());
    QCOMPARE(partN.failedQuestionIds.size(), 7);

    // The other three Prüfungsteile are still passed.
    QCOMPARE(result.parts.at(0).achievedPoints, 25);
    QCOMPARE(result.parts.at(1).achievedPoints, 25);
    QCOMPARE(result.parts.at(3).achievedPoints, 25);
}

/// Six wrong answers per Prüfungsteil are the exact tolerance of the
/// Verfügung: 19 points in 25.
void ExamSessionTest::passMarkAppliesPerPartNotInTotal()
{
    const Catalog catalog = loadClassCatalog("de-amateurfunk-E.xml");
    ExamSession session = fullExam(catalog, 4242u);

    QSet<int> wrong;
    int index = 0;
    for (const QString &letter :
         {QStringLiteral("V"), QStringLiteral("B"), QStringLiteral("N"), QStringLiteral("E")}) {
        for (int k = 0; k < 6; ++k) {
            while (session.partLetterAt(index) != letter)
                ++index;
            wrong.insert(index);
            ++index;
        }
    }
    QCOMPARE(wrong.size(), 24);
    answerAll(&session, wrong);

    const ExamResult result = session.evaluate();
    for (const PartResult &part : result.parts) {
        QCOMPARE(part.achievedPoints, 19);
        QVERIFY(part.passed());
    }
    QCOMPARE(result.achievedPoints, 76);
    QCOMPARE(result.passScore, 76);
    QCOMPARE(result.errorPointBudget, 24);
    QCOMPARE(result.wrongAnswers, 24);
    QVERIFY(result.passed);
    QVERIFY(result.failedParts().isEmpty());
}

void ExamSessionTest::nextUnansweredVisitsEveryQuestionOnce()
{
    const Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    ExamSession session = fullExam(catalog, 5u);
    QCOMPARE(session.count(), 75);
    QCOMPARE(session.nextUnanswered(0), 0);
    QCOMPARE(session.nextUnanswered(-1), 0);
    QCOMPARE(session.nextUnanswered(70), 70);

    QSet<int> visited;
    int index = 0;
    while (true) {
        const int next = session.nextUnanswered(index);
        if (next < 0)
            break;
        QVERIFY(next >= index);
        QVERIFY(!visited.contains(next));
        visited.insert(next);
        session.answer(next, session.correctPositionAt(next));
        index = next;
    }
    QCOMPARE(visited.size(), 75);
    QCOMPARE(session.nextUnanswered(0), -1);
    QCOMPARE(session.answeredCount(), 75);
}

void ExamSessionTest::invalidAnswerPositionsAreIgnored()
{
    const Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    ExamSession session = fullExam(catalog, 6u);

    session.answer(0, -1);
    session.answer(0, 99);
    session.answer(-1, 0);
    session.answer(100000, 0);
    QCOMPARE(session.selectedPosition(0), -1);
    QVERIFY(!session.isAnswered(0));
    QCOMPARE(session.answeredCount(), 0);

    // An index outside the paper must not crash the lookup helpers.
    QVERIFY(session.questionAt(-1) == nullptr);
    QVERIFY(session.questionAt(99999) == nullptr);
    QVERIFY(session.answerOrderAt(99999).isEmpty());
    QVERIFY(session.partLetterAt(99999).isEmpty());
    QCOMPARE(session.selectedPosition(99999), -1);
    QCOMPARE(session.answeredCountOfPart(QStringLiteral("V")), 0);
}

/// The official catalogue states that the correct answer is always A, and
/// that the exam sheets arrange the answers randomly. A learner who notices
/// the first pattern must not be able to pass our simulation by ticking the
/// topmost box.
///
/// Since the import, the catalog itself no longer carries the correct answer
/// at position A either - it is spread over all four places by the same
/// deterministic permutation, so that a hand edited catalog file gives
/// nothing away either. What the test therefore checks is the distribution:
/// in the catalog and again in the drawn paper.
void ExamSessionTest::correctAnswerIsNotAlwaysFirst()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-A.xml");

    // These preconditions are what makes the test meaningful.
    int inCatalog[4] = {0, 0, 0, 0};
    for (const Question *question : catalog.questions()) {
        QVERIFY(question != nullptr);
        QCOMPARE(question->answers.size(), 4);
        int correct = -1;
        for (int i = 0; i < question->answers.size(); ++i) {
            if (question->answers.at(i).correct) {
                QCOMPARE(correct, -1);
                correct = i;
            }
        }
        QVERIFY2(correct >= 0, qPrintable(question->id));
        ++inCatalog[correct];
    }
    for (int position = 0; position < 4; ++position)
        QVERIFY2(inCatalog[position] > 0,
                 qPrintable(QStringLiteral("im Katalog kommt Position %1 nicht vor")
                                .arg(position + 1)));

    int hits[4] = {0, 0, 0, 0};
    int examined = 0;
    // Several papers, because one seed can easily produce a lopsided sample.
    for (quint32 seed = 1; seed <= 40; ++seed) {
        ExamSession session = fullExam(catalog, seed);
        for (int i = 0; i < session.count(); ++i) {
            const int correct = session.correctPositionAt(i);
            QVERIFY(correct >= 0 && correct < 4);
            ++hits[correct];
            ++examined;
        }
    }
    QVERIFY(examined > 4000);

    // A shuffle that never moves, or that always favours one place, would
    // make the learning exercise pointless. Demand a real share everywhere.
    for (int position = 0; position < 4; ++position) {
        const double share = double(hits[position]) / double(examined);
        QVERIFY2(share > 0.18, qPrintable(QStringLiteral("Antwortposition %1 nur %2% erreicht")
                                               .arg(position + 1).arg(share * 100, 0, 'f', 1)));
    }
}

/// A training paper has no official scope, so it must not be graded against
/// the pass marks of the Prüfungsordnung.
void ExamSessionTest::trainingPaperHasNoPassMarks()
{
    const Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    const Scheduler scheduler;
    PaperBuilder::Options options;
    options.questionCount = 12;

    ExamSession session(catalog, PaperBuilder::build(catalog, scheduler, options, 8u));
    QCOMPARE(session.count(), 12);
    QCOMPARE(session.paper().passScore, 0);

    answerAll(&session, {});
    const ExamResult result = session.evaluate();
    QCOMPARE(result.maxPoints, 12);
    QCOMPARE(result.passScore, 0);
    QVERIFY(!result.passed);
    QVERIFY(result.parts.isEmpty());
}

QTEST_MAIN(ExamSessionTest)
#include "tst_exam.moc"