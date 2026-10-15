// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ValidationReport.h"

#include "core/CatalogXml.h"
#include "core/CatalogValidator.h"

#include <QComboBox>
#include <QLabel>
#include <QTreeWidget>
#include <QtTest>

using namespace hrt;

namespace {

QVector<CatalogIssue> mixedIssues()
{
    QVector<CatalogIssue> issues;
    issues.append({IssueSeverity::Error, QStringLiteral("A1"),
                   QStringLiteral("Frage hat keine als richtig markierte Antwort.")});
    issues.append({IssueSeverity::Warning, QStringLiteral("metadata"),
                   QStringLiteral("Hinweis auf die Änderung.")});
    issues.append({IssueSeverity::Warning, QStringLiteral("B2"),
                   QStringLiteral("Frage hat kein Datum 'addedOn' in der Historie.")});
    return issues;
}

/// The report the learner actually sees for a shipped catalog: warnings only.
QVector<CatalogIssue> warningsOfRealCatalog()
{
    const QString base = QStringLiteral(HRT_SOURCE_DIR "/data/catalogs");
    Catalog catalog;
    QString error;
    if (!CatalogXml::load(QDir(base).filePath(QStringLiteral("de-amateurfunk-N.xml")),
                          &catalog, &error))
        qFatal("Katalog nicht lesbar: %s", qPrintable(error));
    QVector<CatalogIssue> issues;
    CatalogValidator::validate(catalog, &issues, base);
    return issues;
}

QLabel *findSummary(ValidationReport *report)
{
    return report->findChild<QLabel *>(QString(), Qt::FindDirectChildrenOnly);
}

QTreeWidget *findList(ValidationReport *report)
{
    return report->findChild<QTreeWidget *>();
}

} // namespace

class ValidationReportTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyReportSaysSo();
    void countsErrorsAndWarnings();
    void filterShowsOnlyTheChosenSeverity();
    void realCatalogHasWarningsButNoErrors();
    void summaryMentionsBothCounts();
};

void ValidationReportTest::emptyReportSaysSo()
{
    ValidationReport report;
    report.setIssues({});

    QCOMPARE(report.errorCount(), 0);
    QCOMPARE(report.warningCount(), 0);
    QCOMPARE(findList(&report)->topLevelItemCount(), 0);
    QVERIFY(report.summary().contains(QStringLiteral("keine Beanstandungen")));
}

void ValidationReportTest::countsErrorsAndWarnings()
{
    ValidationReport report;
    report.setIssues(mixedIssues());

    QCOMPARE(report.errorCount(), 1);
    QCOMPARE(report.warningCount(), 2);
    QCOMPARE(findList(&report)->topLevelItemCount(), 3);
}

void ValidationReportTest::filterShowsOnlyTheChosenSeverity()
{
    ValidationReport report;
    report.setIssues(mixedIssues());

    auto *filter = report.findChild<QComboBox *>();
    QVERIFY(filter);

    filter->setCurrentIndex(1); // nur Fehler
    QCOMPARE(findList(&report)->topLevelItemCount(), 1);

    filter->setCurrentIndex(2); // nur Hinweise
    QCOMPARE(findList(&report)->topLevelItemCount(), 2);

    filter->setCurrentIndex(0); // alle
    QCOMPARE(findList(&report)->topLevelItemCount(), 3);
}

void ValidationReportTest::realCatalogHasWarningsButNoErrors()
{
    const QVector<CatalogIssue> issues = warningsOfRealCatalog();
    QVERIFY(!issues.isEmpty());

    ValidationReport report;
    report.setIssues(issues);

    // Der mitgelieferte Katalog muss benutzbar sein; Hinweise sind erwartet
    // und begruenden keinen Abbruch.
    QCOMPARE(report.errorCount(), 0);
    QVERIFY(report.warningCount() > 0);
    QCOMPARE(findList(&report)->topLevelItemCount(), issues.size());
    QVERIFY(!report.summary().isEmpty());
}

void ValidationReportTest::summaryMentionsBothCounts()
{
    ValidationReport report;
    report.setIssues(mixedIssues());

    QVERIFY(report.summary().contains(QStringLiteral("1")));
    QVERIFY(report.summary().contains(QStringLiteral("2")));
}

QTEST_MAIN(ValidationReportTest)
#include "tst_validationreport.moc"