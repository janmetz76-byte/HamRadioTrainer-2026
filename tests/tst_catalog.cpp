// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "core/Catalog.h"
#include "core/CatalogValidator.h"
#include "core/CatalogXml.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

using namespace hrt;

namespace {

Catalog makeMinimalCatalog()
{
    Catalog catalog;
    catalog.metadata.title = QStringLiteral("Testkatalog");
    catalog.metadata.examClass = ExamClass::Novize;
    catalog.metadata.examId = QStringLiteral("DE-N");
    catalog.metadata.authority = QStringLiteral("Test");
    catalog.metadata.catalogVersion = QStringLiteral("2026-01");
    catalog.metadata.validFrom = QDate(2026, 1, 1);

    Part part;
    part.id = QStringLiteral("DE-N-1");
    part.title = QStringLiteral("Teil 1");
    part.order = 1;

    Section section;
    section.id = QStringLiteral("DE-N-1-1");
    section.title = QStringLiteral("Abschnitt 1");
    section.order = 1;
    part.selection.questions = 1;

    Chapter chapter;
    chapter.id = QStringLiteral("DE-N-1-1-1");
    chapter.title = QStringLiteral("Kapitel 1");
    chapter.order = 1;

    Question question;
    question.id = QStringLiteral("DE-N-1-1-1-001");
    question.order = 1;
    question.text = QStringLiteral("Frage?");
    question.points = 2;
    question.history.addedOn = QDate(2026, 1, 1);
    Answer a;
    a.id = QStringLiteral("a");
    a.text = QStringLiteral("A");
    a.correct = true;
    a.order = 1;
    Answer b;
    b.id = QStringLiteral("b");
    b.text = QStringLiteral("B");
    b.correct = false;
    b.order = 2;
    question.answers = {a, b};
    chapter.questions.append(question);

    section.chapters.append(chapter);
    part.sections.append(section);
    catalog.parts.append(part);
    return catalog;
}

} // namespace

class CatalogTest : public QObject
{
    Q_OBJECT

private slots:
    void examClassRoundTrip();
    void detectsMultipleChoice();
    void serializesAndParsesBack();
    void saveIsDeterministic();
    void generatedXmlPassesSchema();
    void validatorAcceptsMinimalCatalog();
    void validatorRejectsQuestionWithoutCorrectAnswer();
    void validatorRejectsSingleWithTwoCorrectAnswers();
    void validatorRejectsOversizedSelection();
    void validatorDetectsImagePathEscape();
    void loadsShippedExampleCatalogs();
    void shippedCatalogsHaveNoMissingImages();
    void everyClassCoversAllFivePruefungsteile();  
    void sharedPartsAreIdenticalAcrossClasses();
};

void CatalogTest::examClassRoundTrip()
{
    QCOMPARE(examClassToString(ExamClass::Novize), QStringLiteral("N"));
    QCOMPARE(examClassToString(ExamClass::Einsteiger), QStringLiteral("E"));
    QCOMPARE(examClassToString(ExamClass::Allgemein), QStringLiteral("A"));
    QCOMPARE(examClassFromString(QStringLiteral("N")), std::optional(ExamClass::Novize));
    QCOMPARE(examClassFromString(QStringLiteral("a")), std::optional(ExamClass::Allgemein));
    QVERIFY(!examClassFromString(QStringLiteral("B")).has_value());
}

void CatalogTest::detectsMultipleChoice()
{
    Catalog catalog = makeMinimalCatalog();
    Question *question = catalog.questions().first();
    QVERIFY(question->type == QuestionType::Single);
    QVERIFY(!question->isMultipleChoice());
    QCOMPARE(question->correctAnswerIndexes(), QVector<int>{0});

    // The declared type is what the file said, not what the answers imply.
    question->type = QuestionType::Multiple;
    QVERIFY(question->isMultipleChoice());
    question->answers[1].correct = true;
    QCOMPARE(question->correctAnswerIndexes(), (QVector<int>{0, 1}));
}

void CatalogTest::serializesAndParsesBack()
{
    const Catalog original = makeMinimalCatalog();
    Catalog parsed;
    QString error;
    QVERIFY2(CatalogXml::loadFromString(CatalogXml::toXml(original), &parsed, &error),
             qPrintable(error));

    QCOMPARE(parsed.schemaVersion, original.schemaVersion);
    QCOMPARE(parsed.metadata.examClass, ExamClass::Novize);
    QCOMPARE(parsed.parts.size(), 1);
    QCOMPARE(parsed.parts.first().selection.questions, 1);
    QCOMPARE(parsed.parts.first().sections.first().chapters.first().questions.size(), 1);

    const Question *question = parsed.questions().first();
    QCOMPARE(question->id, QStringLiteral("DE-N-1-1-1-001"));
    QCOMPARE(question->text, QStringLiteral("Frage?"));
    QCOMPARE(question->answers.size(), 2);
    QCOMPARE(question->history.addedOn, QDate(2026, 1, 1));
    QVERIFY(question->correctAnswerIndexes().size() >= 1);
}

void CatalogTest::saveIsDeterministic()
{
    const Catalog catalog = makeMinimalCatalog();
    QCOMPARE(CatalogXml::toXml(catalog), CatalogXml::toXml(catalog));

    QTemporaryDir dir;
    QString error;
    const QString first = dir.filePath(QStringLiteral("a.xml"));
    const QString second = dir.filePath(QStringLiteral("b.xml"));
    QVERIFY2(CatalogXml::save(first, catalog, &error), qPrintable(error));
    QVERIFY2(CatalogXml::save(second, catalog, &error), qPrintable(error));
    auto readFile = [](const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    QCOMPARE(readFile(first), readFile(second));
}

void CatalogTest::generatedXmlPassesSchema()
{
    const QByteArray document = CatalogXml::toXml(makeMinimalCatalog());

    QDomDocument probe;
    QVERIFY2(probe.setContent(document), "erzeugtes XML ist nicht wohlgeformt");
    QCOMPARE(probe.documentElement().tagName(), QStringLiteral("catalog"));
    QCOMPARE(probe.documentElement().attribute(QStringLiteral("schemaVersion")),
             QStringLiteral("1"));
    // No encryption, no obfuscation: the payload must stay readable text.
    QVERIFY(document.contains("<text>Frage?</text>"));
}

void CatalogTest::validatorAcceptsMinimalCatalog()
{
    QVector<CatalogIssue> issues;
    QVERIFY(CatalogValidator::validate(makeMinimalCatalog(), &issues));
    for (const CatalogIssue &issue : issues)
        QCOMPARE(issue.severity, IssueSeverity::Warning);
}

void CatalogTest::validatorRejectsQuestionWithoutCorrectAnswer()
{
    Catalog catalog = makeMinimalCatalog();
    catalog.questions().first()->answers[0].correct = false;

    QVector<CatalogIssue> issues;
    QVERIFY(!CatalogValidator::validate(catalog, &issues));
    QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const CatalogIssue &issue) {
        return issue.message.contains(QStringLiteral("keine als richtig markierte"));
    }));
}

void CatalogTest::validatorRejectsSingleWithTwoCorrectAnswers()
{
    Catalog catalog = makeMinimalCatalog();
    catalog.questions().first()->answers[1].correct = true;

    QVector<CatalogIssue> issues;
    QVERIFY(!CatalogValidator::validate(catalog, &issues));
    QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const CatalogIssue &issue) {
        return issue.message.contains(QStringLiteral("als 'single' deklariert"));
    }));
}

void CatalogTest::validatorRejectsOversizedSelection()
{
    Catalog catalog = makeMinimalCatalog();
    catalog.parts.first().selection.questions = 99;

    QVector<CatalogIssue> issues;
    QVERIFY(!CatalogValidator::validate(catalog, &issues));
    QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const CatalogIssue &issue) {
        return issue.message.contains(QStringLiteral("verfügbare"));
    }));
}

void CatalogTest::validatorDetectsImagePathEscape()
{
    Catalog catalog = makeMinimalCatalog();
    catalog.questions().first()->image = QStringLiteral("../../etc/passwd");

    QVector<CatalogIssue> issues;
    QVERIFY(!CatalogValidator::validate(catalog, &issues));
    QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const CatalogIssue &issue) {
        return issue.message.contains(QStringLiteral("muss relativ sein"));
    }));
}

void CatalogTest::loadsShippedExampleCatalogs()
{
    const QString base = QStringLiteral(HRT_SOURCE_DIR "/data/catalogs");
    const QStringList files = {QStringLiteral("de-amateurfunk-N.xml"),
                               QStringLiteral("de-amateurfunk-E.xml"),
                               QStringLiteral("de-amateurfunk-A.xml")};

    for (const QString &file : files) {
        const QString path = QDir(base).filePath(file);
        QVERIFY2(QFileInfo::exists(path), qPrintable(QStringLiteral("fehlt: %1").arg(path)));

        Catalog catalog;
        QString error;
        QVERIFY2(CatalogXml::load(path, &catalog, &error), qPrintable(error));

        QVector<CatalogIssue> issues;
        const bool usable = CatalogValidator::validate(catalog, &issues, base);
        for (const CatalogIssue &issue : issues) {
            qWarning("%s [%s] %s", qPrintable(file), qPrintable(issue.location),
                      qPrintable(issue.message));
        }
        QVERIFY2(usable, qPrintable(QStringLiteral("%1 ist nicht verwendbar").arg(file)));
        QVERIFY(catalog.activeQuestionCount() > 0);
    }
}

/// Amtsblatt-Verfügung 29/2024 defines five sheets: V and B shared by all
/// classes, plus one technical sheet per class N, E, A. A certificate covers
/// all sheets up to and including its own class, because a candidate who
/// already holds a lower certificate only sits the sheets above it.
void CatalogTest::shippedCatalogsHaveNoMissingImages()
{
    // Regression: Der Validator bekommt ein Verzeichnis. Uebergab man ihm
    // stattdessen den Katalogdateipfad, meldete er jedes der 687 Bilder als
    // fehlend - die Anzeige war dann voll von Warnungen, obwohl alle Bilder
    // vorhanden waren. loadsShippedExampleCatalogs haette das durchgelassen,
    // weil es die Hinweise nur protokolliert.
    const QString base = QStringLiteral(HRT_SOURCE_DIR "/data/catalogs");
    const QStringList files = {QStringLiteral("de-amateurfunk-N.xml"),
                               QStringLiteral("de-amateurfunk-E.xml"),
                               QStringLiteral("de-amateurfunk-A.xml")};

    for (const QString &file : files) {
        Catalog catalog;
        QString error;
        QVERIFY2(CatalogXml::load(QDir(base).filePath(file), &catalog, &error),
                 qPrintable(error));

        QVector<CatalogIssue> issues;
        CatalogValidator::validate(catalog, &issues, base);

        for (const CatalogIssue &issue : issues) {
            QVERIFY2(!issue.message.contains(QStringLiteral("nicht gefunden")),
                     qPrintable(QStringLiteral("%1 meldet ein fehlendes Bild: %2 (Basispfad %3)")
                                    .arg(file, issue.message, base)));
        }
    }
}

void CatalogTest::everyClassCoversAllFivePruefungsteile()
{
    const QString base = QStringLiteral(HRT_SOURCE_DIR "/data/catalogs");

    struct Expectation
    {
        const char *file;
        QStringList sheets;   ///< letters in expected order
        int questions;
    };
    const QVector<Expectation> expectations = {
        {"de-amateurfunk-N.xml", {QStringLiteral("V"), QStringLiteral("B"), QStringLiteral("N")}, 571},
        {"de-amateurfunk-E.xml",
         {QStringLiteral("V"), QStringLiteral("B"), QStringLiteral("N"), QStringLiteral("E")}, 1034},
        {"de-amateurfunk-A.xml",
         {QStringLiteral("V"), QStringLiteral("B"), QStringLiteral("N"), QStringLiteral("E"),
          QStringLiteral("A")},
         1750},
    };

    for (const Expectation &expectation : expectations) {
        Catalog catalog;
        QString error;
        const QString path = QDir(base).filePath(QString::fromLatin1(expectation.file));
        QVERIFY2(CatalogXml::load(path, &catalog, &error), qPrintable(error));

        QCOMPARE(catalog.parts.size(), expectation.sheets.size());
        int total = 0;
        for (int i = 0; i < catalog.parts.size(); ++i) {
            const Part &part = catalog.parts.at(i);
            const QString letter = part.id.section(QLatin1Char('-'), -1).toUpper();
            QCOMPARE(letter, expectation.sheets.at(i));

            // Prüfungsumfang: 25 questions, 6 tolerated wrong answers, 19 points,
            // 60 minutes only for sheet A.
            QCOMPARE(part.selection.questions, 25);
            QCOMPARE(part.selection.errorPoints.value_or(-1), 6);
            QCOMPARE(part.selection.passScore.value_or(-1), 19);
            QCOMPARE(part.selection.minutes.value_or(-1), letter == QLatin1String("A") ? 60 : 45);

            QVERIFY(!part.sections.isEmpty());
            for (const Section &section : part.sections)
                total += section.activeQuestionCount();
        }
        QCOMPARE(total, expectation.questions);

        // One point per question, taken from the Verfügung.
        QCOMPARE(catalog.metadata.defaultPoints, 1);
        QVERIFY(!catalog.metadata.attribution.isEmpty());
        QCOMPARE(catalog.metadata.license, QStringLiteral("DL-DE-BY-2.0"));
    }
}

/// V and B carry no class tag in the source, and the technical sheet of a
/// lower class is reused unchanged in the catalogs of the higher classes.
/// All of them must therefore carry identical question sets.
void CatalogTest::sharedPartsAreIdenticalAcrossClasses()
{
    const QString base = QStringLiteral(HRT_SOURCE_DIR "/data/catalogs");
    QHash<QString, QSet<QString>> perSheet; // sheet letter -> question ids
    int technicalTotal = 0;

    for (const QString &file :
         {QStringLiteral("de-amateurfunk-N.xml"), QStringLiteral("de-amateurfunk-E.xml"),
          QStringLiteral("de-amateurfunk-A.xml")}) {
        Catalog catalog;
        QString error;
        QVERIFY2(CatalogXml::load(QDir(base).filePath(file), &catalog, &error), qPrintable(error));

        for (const Part &part : catalog.parts) {
            const QString letter = part.id.section(QLatin1Char('-'), -1).toUpper();
            QSet<QString> ids;
            for (const Section &section : part.sections) {
                for (const Question *question : section.questions())
                    ids.insert(question->id);
            }
            if (!perSheet.contains(letter))
                perSheet.insert(letter, ids);
            QCOMPARE(ids, perSheet.value(letter));
        }
    }

    QCOMPARE(perSheet.size(), 5);
    QCOMPARE(perSheet.value(QStringLiteral("V")).size(), 204);
    QCOMPARE(perSheet.value(QStringLiteral("B")).size(), 172);
    QCOMPARE(perSheet.value(QStringLiteral("N")).size(), 195);
    QCOMPARE(perSheet.value(QStringLiteral("E")).size(), 463);
    QCOMPARE(perSheet.value(QStringLiteral("A")).size(), 716);

    for (const QString &letter : {QStringLiteral("V"), QStringLiteral("B"),
                                  QStringLiteral("N")})
        technicalTotal += perSheet.value(letter).size();
    QCOMPARE(technicalTotal, 571); // pool of a class N certificate
}

QTEST_MAIN(CatalogTest)
#include "tst_catalog.moc"