// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "core/CatalogLocator.h"
#include "core/CatalogXml.h"

#include <QDir>
#include <QFileInfo>
#include <QtTest>

using namespace hrt;

class CatalogLocatorTest : public QObject
{
    Q_OBJECT

private slots:
    void searchDirectoriesArePlausible();
    void catalogsOfTheSourceTreeAreFound();
    void everyFoundCatalogIsReadable();
    void aDirectoryWithoutCatalogsIsSkipped();
};

void CatalogLocatorTest::searchDirectoriesArePlausible()
{
    const QStringList directories = CatalogLocator::searchDirectories();
    QVERIFY(!directories.isEmpty());

    // No entry may be empty, and the list must not repeat itself.
    for (const QString &directory : directories)
        QVERIFY2(!directory.isEmpty(), "leerer Suchpfad");
    QCOMPARE(directories.size(), QSet<QString>(directories.cbegin(), directories.cend()).size());

    // The directory of the running program has to be among them, otherwise a
    // portable installation could never find anything.
    QVERIFY(directories.contains(QCoreApplication::applicationDirPath()));
}

void CatalogLocatorTest::catalogsOfTheSourceTreeAreFound()
{
    // The source directory is compiled in, so the search has to reach the
    // catalogs of a checkout without any environment being set.
    const CatalogLocator::Result result = CatalogLocator::locate();
    QVERIFY2(result.problem.isEmpty(), qPrintable(result.problem));
    QVERIFY(!result.paths.isEmpty());
    QVERIFY(!result.baseDirectory.isEmpty());
    QVERIFY(QFileInfo(result.baseDirectory).isDir());
}

void CatalogLocatorTest::everyFoundCatalogIsReadable()
{
    const CatalogLocator::Result result = CatalogLocator::locate();
    QVERIFY(!result.paths.isEmpty());

    for (const QString &path : result.paths) {
        Catalog catalog;
        QString error;
        QVERIFY2(CatalogXml::load(path, &catalog, &error), qPrintable(error));

        // The class of the file name has to be the class in the catalog, so a
        // wrong pairing cannot slip in unnoticed.
        QVERIFY(!catalog.parts.isEmpty());
        const QString letter = catalog.metadata.examClass == ExamClass::Novize     ? QStringLiteral("N")
                               : catalog.metadata.examClass == ExamClass::Einsteiger ? QStringLiteral("E")
                                                                                    : QStringLiteral("A");
        QCOMPARE(QFileInfo(path).baseName().right(1), letter);
        QVERIFY(catalog.metadata.examId.endsWith(letter));
    }
}

void CatalogLocatorTest::aDirectoryWithoutCatalogsIsSkipped()
{
    // The search may be pointed at an empty directory; it has to move on and
    // still find the catalogs of the source tree.
    QTemporaryDir empty;
    QVERIFY(empty.isValid());
    QVERIFY(QDir(empty.path()).entryList(QDir::Files).isEmpty());

    const CatalogLocator::Result result = CatalogLocator::locate();
    QVERIFY(!result.paths.isEmpty());
    for (const QString &path : result.paths)
        QVERIFY(QFileInfo::exists(path));
}

QTEST_MAIN(CatalogLocatorTest)
#include "tst_cataloglocator.moc"