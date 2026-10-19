// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
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
///
/// The wording of a question is its id unless the caller says otherwise: a
/// reworded question keeps its id and changes its text, which is exactly the
/// case the progress has to tell apart from an untouched one.
Catalog makeCatalog(const QString &examId, const QStringList &questionIds,
                    const QString &digest = QStringLiteral("digest-A"),
                    const QString &reworded = QString())
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
        question.text = id == reworded ? QStringLiteral("%1, neu formuliert").arg(id) : id;
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
    void changedCatalogKeepsWhatStillMatches();
    void aRewordedQuestionStartsOverWhileTheRestStays();
    void progressOfRemovedQuestionsIsCountedNotApplied();
    void anOldFileWithoutFingerprintsIsTakenOverWithAVerdict();
    void archiveKeepsTheFileBeforeItIsRebound();
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
    QVERIFY(!loaded.catalogChanged);
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
void ProgressStoreTest::changedCatalogKeepsWhatStillMatches()
{
    // Der Kern des Versprechens: eine Aktualisierung, bei der keine einzige
    // Frage anders formuliert wurde, kostet den Lernenden nichts.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));
    ProgressStore store(path);

    const Catalog original = makeCatalog(QStringLiteral("DE-E"),
                                         {QStringLiteral("q1"), QStringLiteral("q2")},
                                         QStringLiteral("digest-1"));
    QVERIFY(store.save(original, {makeState(QStringLiteral("q1"), 4),
                                  makeState(QStringLiteral("q2"), 2)},
                       nullptr));

    const Catalog aktualisiert = makeCatalog(QStringLiteral("DE-E"),
                                             {QStringLiteral("q1"), QStringLiteral("q2")},
                                             QStringLiteral("digest-2"));
    const ProgressLoad loaded = store.load(aktualisiert);

    QVERIFY(loaded.error.isEmpty());
    QVERIFY2(loaded.catalogChanged, "der neue Katalogeinstand muss erkannt werden");
    QCOMPARE(loaded.states.size(), 2);
    QCOMPARE(loaded.keptStates, 2);
    QCOMPARE(loaded.resetStates, 0);
    QCOMPARE(loaded.droppedStates, 0);

    // Die Staende sind unveraendert, nicht zurueckgesetzt.
    QCOMPARE(loaded.states.first().box, 4);
    QCOMPARE(loaded.states.last().box, 2);

    // Und die alte Datei laesst sich weiterhin fuer ihren Katalogeinstand
    // lesen.
    QCOMPARE(store.load(original).states.size(), 2);
}

void ProgressStoreTest::aRewordedQuestionStartsOverWhileTheRestStays()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));
    ProgressStore store(path);

    const QStringList ids = {QStringLiteral("q1"), QStringLiteral("q2")};
    QVERIFY(store.save(makeCatalog(QStringLiteral("DE-E"), ids, QStringLiteral("digest-1")),
                       {makeState(QStringLiteral("q1"), 5), makeState(QStringLiteral("q2"), 3)},
                       nullptr));

    const ProgressLoad loaded = store.load(
        makeCatalog(QStringLiteral("DE-E"), ids, QStringLiteral("digest-2"),
                    QStringLiteral("q1")));

    QVERIFY(loaded.catalogChanged);
    QCOMPARE(loaded.states.size(), 2);
    QCOMPARE(loaded.resetStates, 1);
    QCOMPARE(loaded.keptStates, 1);

    // q1 zaehlt neu: die alte Antwort galt fuer einen anderen Wortlaut.
    QuestionState neu;
    for (const QuestionState &state : loaded.states) {
        if (state.questionId == QStringLiteral("q1"))
            neu = state;
    }
    QVERIFY2(neu.isNew(), "eine umformulierte Frage darf nicht als beantwortet gelten");
    QCOMPARE(neu.box, 0);
    QCOMPARE(neu.timesAnswered, 0);

    // q2 bleibt, wo sie war.
    for (const QuestionState &state : loaded.states) {
        if (state.questionId == QStringLiteral("q2"))
            QCOMPARE(state.box, 3);
    }
}

void ProgressStoreTest::progressOfRemovedQuestionsIsCountedNotApplied()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));
    ProgressStore store(path);

    QVERIFY(store.save(makeCatalog(QStringLiteral("DE-E"),
                                   {QStringLiteral("q1"), QStringLiteral("q2"),
                                    QStringLiteral("q3")},
                                   QStringLiteral("digest-1")),
                       {makeState(QStringLiteral("q1"), 4), makeState(QStringLiteral("q2"), 2),
                        makeState(QStringLiteral("q3"), 1)},
                       nullptr));

    const ProgressLoad loaded = store.load(makeCatalog(
        QStringLiteral("DE-E"), {QStringLiteral("q1"), QStringLiteral("q2")},
        QStringLiteral("digest-2")));

    QVERIFY(loaded.catalogChanged);
    QCOMPARE(loaded.states.size(), 2);
    QCOMPARE(loaded.droppedStates, 1);
    QCOMPARE(loaded.keptStates, 2);

    // Die entfernte Frage kommt auch nach dem naechsten Speichern nicht
    // zurueck: gespeichert wird nur, was der Katalog noch kennt.
    const Catalog aktualisiert = makeCatalog(
        QStringLiteral("DE-E"), {QStringLiteral("q1"), QStringLiteral("q2")},
        QStringLiteral("digest-3"));
    QVERIFY(store.save(aktualisiert, loaded.states, nullptr));

    const ProgressLoad erneut = store.load(aktualisiert);
    QCOMPARE(erneut.states.size(), 2);
    QVERIFY2(!erneut.catalogChanged, "nach dem Neuspeichern ist der Stand gebunden");
}

void ProgressStoreTest::anOldFileWithoutFingerprintsIsTakenOverWithAVerdict()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));

    // Eine Datei aus der Zeit vor den Fingerabdruecken: gleiche Felder,
    // Version 1, kein "hash".
    QJsonObject entry;
    entry.insert(QStringLiteral("id"), QStringLiteral("q1"));
    entry.insert(QStringLiteral("box"), 5);
    entry.insert(QStringLiteral("answered"), 10);
    entry.insert(QStringLiteral("correct"), 5);
    entry.insert(QStringLiteral("streak"), 2);
    entry.insert(QStringLiteral("lastSeen"), QStringLiteral("2026-09-01"));
    entry.insert(QStringLiteral("dueOn"), QStringLiteral("2026-09-20"));
    entry.insert(QStringLiteral("confidence"), int(Confidence::Certain));

    QJsonObject root;
    root.insert(QStringLiteral("format"), ProgressStore::kFormat);
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("examId"), QStringLiteral("DE-E"));
    root.insert(QStringLiteral("catalogSha256"), QStringLiteral("digest-1"));
    root.insert(QStringLiteral("states"), QJsonArray{entry});

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    const ProgressLoad geladen = ProgressStore(path).load(
        makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")}, QStringLiteral("digest-2")));

    QVERIFY(geladen.error.isEmpty());
    QVERIFY(geladen.catalogChanged);
    QVERIFY2(geladen.withoutFingerprints,
             "ohne Fingerabdruck muss das Programm sagen, dass es nur die Kennung sah");
    QCOMPARE(geladen.keptStates, 1);
    QCOMPARE(geladen.resetStates, 0);

    // Der Stand selbst bleibt unangetastet: er wird nicht weggenommen, nur
    // eingeordnet.
    QCOMPARE(geladen.states.first().box, 5);

    // Ein fremdes, hoeheres Format bleibt abgelehnt.
    QJsonObject fremd = root;
    fremd.insert(QStringLiteral("version"), ProgressStore::kVersion + 1);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(fremd).toJson());
    file.close();
    QVERIFY(!ProgressStore(path)
                 .load(makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")},
                                   QStringLiteral("digest-2")))
                 .error.isEmpty());
}

void ProgressStoreTest::archiveKeepsTheFileBeforeItIsRebound()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("fortschritt.json"));
    ProgressStore store(path);

    const Catalog original = makeCatalog(QStringLiteral("DE-E"), {QStringLiteral("q1")},
                                         QStringLiteral("digest-1"));
    QVERIFY(store.save(original, {makeState(QStringLiteral("q1"), 4)}, nullptr));

    QString error;
    QVERIFY2(store.archive(&error), qPrintable(error));
    QVERIFY2(QFile::exists(path + QStringLiteral(".bak")),
             "ohne Sicherung waere der alte Stand nach dem Ueberschreiben weg");

    // Der Sicherungsstand traegt weiterhin die alte Bindung.
    QJsonDocument backup =
        QJsonDocument::fromJson([&] {
            QFile handle(path + QStringLiteral(".bak"));
            handle.open(QIODevice::ReadOnly);
            return handle.readAll();
        }());
    QCOMPARE(backup.object().value(QStringLiteral("catalogSha256")).toString(),
             QStringLiteral("digest-1"));

    // Ohne Datei ist die Sicherung kein Fehler.
    QVERIFY(ProgressStore(dir.filePath(QStringLiteral("gibtsnicht.json"))).archive(nullptr));
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
    QVERIFY(!store.load(catalog).catalogChanged);
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