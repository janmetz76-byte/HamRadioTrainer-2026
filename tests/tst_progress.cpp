// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "learning/ProgressStore.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace hrt;

namespace {

/// A catalog with the given question ids, standing in for a hand editable file.
Catalog makeCatalog(const QString &examId, const QStringList &questionIds,
                    const QString &digest = QStringLiteral("digest-A"))
{
    Catalog catalog;
    catalog.metadata.examId = examId;
    catalog.metadata.examClass = ExamClass::Einsteiger;
    catalog.sourceDigest = digest;

    Part part;
    part.id = examId + QStringLiteral("-teil1");
    part.title = QStringLiteral("Technische Kenntnisse");
    part.order = 1;

    Section section;
    section.id = part.id + QStringLiteral("-abschnitt1");
    section.title = QStringLiteral("Abschnitt");
    section.order = 1;

    Chapter chapter;
    chapter.id = section.id + QStringLiteral("-kapitel1");
    chapter.title = QStringLiteral("Kapitel");
    chapter.order = 1;

    for (const QString &id : questionIds) {
        Question question;
        question.id = id;
        question.text = id;
        chapter.questions.append(question);
    }
    section.chapters.append(chapter);
    part.sections.append(section);
    catalog.parts.append(part);
    return catalog;
}

QuestionState makeState(const QString &id, int box)
{
    QuestionState state;
    state.questionId = id;
    state.box = box;
    state.timesAnswered = box * 2;
    state.timesCorrect = box;
    state.consecutiveCorrect = box;
    state.lastSeen = QDate(2026, 9, 1);
    state.dueOn = QDate(2026, 9, 20);
    state.confidence = Confidence::FairlySure;
    return state;
}

} // namespace

class ProgressStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void missingFileIsNotAnError();
    void progressSurvivesARoundTrip();
    void unknownQuestionsAreNotSaved();
    void statesOfRemovedQuestionsAreDropped();
    void changedCatalogDiscardsTheProgress();
    void foreignFileIsRejected();
    void wrongVersionIsRejected();
    void foreignCatalogIsRejected();
    void corruptFileIsRejected();
    void impossibleValuesAreClamped();
    void clearEmptiesTheProgress();
    void defaultPathDependsOnTheExam();
};

void ProgressStoreTest::init()
{
}

void ProgressStoreTest::missingFileIsNotAnError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const Catalog catalog = makeCatalog(QStringLiteral("DE-E"),
                                        {QStringLiteral("q1"), QStringLiteral("q2")});

    const ProgressStore store(dir.filePath(QStringLiteral("fortschritt.json")));
    const ProgressLoad loaded = store.load(catalog);

    QVERIFY(loaded.error.isEmpty());
    QVERIFY(loaded.states.isEmpty());
    QVERIFY(!loaded.discardedDueToCatalogChange);
}

void ProgressStoreTest::progressSurvivesARoundTrip()
{
    QTemporaryDir dir;
    const Catalog catalog = makeCatalog(QStringLiteral("DE-E"),
                                        {QStringLiteral("q1"), QStringLiteral("q2")});
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));
    const ProgressStore store(path);

    QString error;
    QVERIFY2(store.save(catalog, {makeState(QStringLiteral("q1"), 3),
                                  makeState(QStringLiteral("q2"), 5)},
                       &error),
             qPrintable(error));

    // A fresh store, so nothing can survive in memory.
    const ProgressStore reopened(path);
    const ProgressLoad loaded = reopened.load(catalog);
    QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
    QCOMPARE(loaded.states.size(), 2);
    QCOMPARE(loaded.droppedStates, 0);

    const QuestionState first = loaded.states.at(0);
    QCOMPARE(first.questionId, QStringLiteral("q1"));
    QCOMPARE(first.box, 3);
    QCOMPARE(first.timesAnswered, 6);
    QCOMPARE(first.timesCorrect, 3);
    QCOMPARE(first.consecutiveCorrect, 3);
    QCOMPARE(first.lastSeen, QDate(2026, 9, 1));
    QCOMPARE(first.dueOn, QDate(2026, 9, 20));
    QCOMPARE(first.confidence, Confidence::FairlySure);
}

void ProgressStoreTest::unknownQuestionsAreNotSaved()
{
    QTemporaryDir dir;
    const Catalog catalog = makeCatalog(QStringLiteral("DE-N"), {QStringLiteral("q1")});
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));
    const ProgressStore store(path);

    QString error;
    QVERIFY(store.save(catalog,
                       {makeState(QStringLiteral("q1"), 2), makeState(QStringLiteral("weg"), 4)},
                       &error));

    const ProgressLoad loaded = store.load(catalog);
    QVERIFY(loaded.error.isEmpty());
    QCOMPARE(loaded.states.size(), 1);
    QCOMPARE(loaded.states.first().questionId, QStringLiteral("q1"));
}

/// A question removed by hand from the catalog must not keep its state.
void ProgressStoreTest::statesOfRemovedQuestionsAreDropped()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));

    Catalog before = makeCatalog(QStringLiteral("DE-N"),
                                 {QStringLiteral("q1"), QStringLiteral("q2")});
    ProgressStore store(path);
    QVERIFY(store.save(before, {makeState(QStringLiteral("q1"), 3),
                                makeState(QStringLiteral("q2"), 3)}, nullptr));

    // The same catalog, one question removed.
    const Catalog after = makeCatalog(QStringLiteral("DE-N"), {QStringLiteral("q1")});
    const ProgressLoad loaded = store.load(after);
    QVERIFY(loaded.error.isEmpty());
    QCOMPARE(loaded.states.size(), 1);
    QCOMPARE(loaded.states.first().questionId, QStringLiteral("q1"));
    QCOMPARE(loaded.droppedStates, 1);
}

/// The catalog is meant to be edited by hand, so progress of an earlier
/// revision must not be applied to questions that may have changed.
void ProgressStoreTest::changedCatalogDiscardsTheProgress()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));
    ProgressStore store(path);

    const Catalog original = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")},
                                         QStringLiteral("digest-1"));
    QVERIFY(store.save(original, {makeState(QStringLiteral("q1"), 5)}, nullptr));

    const Catalog edited = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")},
                                       QStringLiteral("digest-2"));
    const ProgressLoad loaded = store.load(edited);
    QVERIFY2(loaded.discardedDueToCatalogChange, "geänderter Katalog muss den Fortschritt verwerfen");
    QVERIFY(loaded.states.isEmpty());
    QVERIFY(loaded.error.isEmpty());

    // The old revision still loads its own progress.
    QCOMPARE(store.load(original).states.size(), 1);
}

void ProgressStoreTest::foreignFileIsRejected()
{
    QTemporaryDir dir;
    const Catalog catalog = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")});
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(QJsonObject{{QStringLiteral("something"), QStringLiteral("else")}})
                   .toJson());
    file.close();

    const ProgressLoad loaded = ProgressStore(path).load(catalog);
    QVERIFY(!loaded.error.isEmpty());
    QVERIFY(loaded.states.isEmpty());
}

void ProgressStoreTest::wrongVersionIsRejected()
{
    QTemporaryDir dir;
    const Catalog catalog = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")});
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));

    QJsonObject root;
    root.insert(QStringLiteral("format"), ProgressStore::kFormat);
    root.insert(QStringLiteral("version"), 99);
    root.insert(QStringLiteral("examId"), QStringLiteral("DE-E"));
    root.insert(QStringLiteral("catalogSha256"), catalog.sourceDigest);

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    const ProgressLoad loaded = ProgressStore(path).load(catalog);
    QVERIFY2(!loaded.error.isEmpty(), "eine fremde Version darf nicht gelesen werden");
    QVERIFY(loaded.states.isEmpty());
}

void ProgressStoreTest::foreignCatalogIsRejected()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));

    const Catalog n = makeCatalog(QStringLiteral("DE-N"), {QStringLiteral("q1")});
    ProgressStore store(path);
    QVERIFY(store.save(n, {makeState(QStringLiteral("q1"), 2)}, nullptr));

    // Same digest, but a different exam: the file belongs to someone else.
    const Catalog other = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")});
    const ProgressLoad loaded = store.load(other);
    QVERIFY(!loaded.error.isEmpty());
    QVERIFY(loaded.states.isEmpty());
}

void ProgressStoreTest::corruptFileIsRejected()
{
    QTemporaryDir dir;
    const Catalog catalog = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")});
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ das ist kein JSON");
    file.close();

    const ProgressLoad loaded = ProgressStore(path).load(catalog);
    QVERIFY(!loaded.error.isEmpty());
    QVERIFY(loaded.states.isEmpty());
}

void ProgressStoreTest::impossibleValuesAreClamped()
{
    QTemporaryDir dir;
    const Catalog catalog = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")});
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));

    QJsonObject state;
    state.insert(QStringLiteral("id"), QStringLiteral("q1"));
    state.insert(QStringLiteral("box"), 99);
    state.insert(QStringLiteral("answered"), -5);
    state.insert(QStringLiteral("correct"), -1);
    state.insert(QStringLiteral("streak"), -2);
    state.insert(QStringLiteral("confidence"), 42);

    QJsonObject root;
    root.insert(QStringLiteral("format"), ProgressStore::kFormat);
    root.insert(QStringLiteral("version"), ProgressStore::kVersion);
    root.insert(QStringLiteral("examId"), QStringLiteral("DE-E"));
    root.insert(QStringLiteral("catalogSha256"), catalog.sourceDigest);
    root.insert(QStringLiteral("states"), QJsonArray{state});

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    const ProgressLoad loaded = ProgressStore(path).load(catalog);
    QCOMPARE(loaded.states.size(), 1);
    const QuestionState loadedState = loaded.states.first();
    QCOMPARE(loadedState.box, Scheduler::MaxBox);
    QCOMPARE(loadedState.timesAnswered, 0);
    QCOMPARE(loadedState.timesCorrect, 0);
    QCOMPARE(loadedState.consecutiveCorrect, 0);
    QCOMPARE(loadedState.confidence, Confidence::Unknown);
}

void ProgressStoreTest::clearEmptiesTheProgress()
{
    QTemporaryDir dir;
    const Catalog catalog = makeCatalog(QStringLiteral("DE-A"), {QStringLiteral("q1")});
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));
    ProgressStore store(path);

    QVERIFY(store.save(catalog, {makeState(QStringLiteral("q1"), 4)}, nullptr));
    QCOMPARE(store.load(catalog).states.size(), 1);

    QString error;
    QVERIFY2(store.clear(catalog, &error), qPrintable(error));
    QVERIFY(store.load(catalog).states.isEmpty());

    // The file still exists and stays bound to the current catalog.
    QVERIFY(QFile::exists(path));
    QVERIFY(!store.load(catalog).discardedDueToCatalogChange);
}

void ProgressStoreTest::defaultPathDependsOnTheExam()
{
    const Catalog n = makeCatalog(QStringLiteral("DE-N"), {QStringLiteral("q1")});
    const Catalog e = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")});

    const QString nPath = ProgressStore::defaultPath(n);
    const QString ePath = ProgressStore::defaultPath(e);
    QVERIFY(!nPath.isEmpty());
    QVERIFY(nPath != ePath);
    QVERIFY(nPath.endsWith(QStringLiteral("fortschritt-de-n.json")));
    QVERIFY(QFileInfo(nPath).absolutePath().startsWith(
        QFileInfo(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .absolutePath()));
}

QTEST_MAIN(ProgressStoreTest)
#include "tst_progress.moc"