// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Prueft den Sollstand der mitgelieferten Kataloge: die Pruefsummen muessen zu
// den Dateien passen, und die Abweichungserkennung muss handgeaenderte Kataloge
// von den ausgelieferten unterscheiden.
#include <QtTest>

#include "core/Catalog.h"
#include "core/CatalogManifest.h"
#include "core/CatalogXml.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace hrt;

class CatalogManifestTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    /// Der Sollstand darf nicht veralten: sonst meldet das Programm einen
    /// frisch ausgelieferten Katalog als "von Hand geaendert".
    void theShippedManifestMatchesTheShippedCatalogs();
    void anUnreadableManifestLeavesNoClaim();
    void anUntouchedCatalogIsRecognisedAsShipped();
    void aHandChangedCatalogIsRecognisedAsEdited();
    void theDescriptionNamesTheEdition();
    /// Regression: ohne sourceDigest vergleicht der Fortschritt zweier
    /// leerer Zeichenketten und haelt seinen Stand bei geaendertem Katalog.
    void loadingACatalogComputesItsDigest();

private:
    QString catalogDirectory() const { return QString::fromUtf8(HRT_SOURCE_DIR) + QStringLiteral("/data/catalogs"); }
    QString path(const QString &name) const { return QDir(catalogDirectory()).filePath(name); }

    QDir m_catalogDir;
};

void CatalogManifestTest::init()
{
    m_catalogDir = QDir(catalogDirectory());
    QVERIFY2(m_catalogDir.exists(), qPrintable(catalogDirectory()));
}

void CatalogManifestTest::theShippedManifestMatchesTheShippedCatalogs()
{
    const CatalogManifest manifest = CatalogManifest::load(catalogDirectory());
    QVERIFY2(!manifest.isEmpty(), "data/catalogs/catalog-manifest.json fehlt oder ist leer");

    QCOMPARE(manifest.entries().size(), 3);
    for (const CatalogManifestEntry &entry : manifest.entries()) {
        const QString file = m_catalogDir.filePath(entry.file);
        QVERIFY2(QFile::exists(file), qPrintable(entry.file));

        QFile handle(file);
        QVERIFY(handle.open(QIODevice::ReadOnly));
        const QString digest = QString::fromLatin1(
            QCryptographicHash::hash(handle.readAll(), QCryptographicHash::Sha256).toHex());

        QVERIFY2(digest == entry.sha256,
                 qPrintable(QStringLiteral("%1: Sollstand %2, Datei %3")
                                .arg(entry.file, entry.sha256, digest)));
    }
}

void CatalogManifestTest::anUnreadableManifestLeavesNoClaim()
{
    QTemporaryDir leer;
    QVERIFY(leer.isValid());

    const CatalogManifest manifest = CatalogManifest::load(leer.path());
    QVERIFY(manifest.isEmpty());

    Catalog catalog;
    QString error;
    QVERIFY(CatalogXml::load(path(QStringLiteral("de-amateurfunk-N.xml")), &catalog, &error));
    // Ohne Sollstand sagt das Programm "ich weiss es nicht" und erfindet
    // keinen Vergleich.
    QCOMPARE(manifest.driftOf(catalog, leer.path()), CatalogDrift::Unknown);
    QVERIFY(CatalogManifest::describe(CatalogDrift::Unknown, catalog, CatalogManifestEntry())
                .contains(QStringLiteral("kein Sollstand")));
}

void CatalogManifestTest::anUntouchedCatalogIsRecognisedAsShipped()
{
    const CatalogManifest manifest = CatalogManifest::load(catalogDirectory());
    QVERIFY(!manifest.isEmpty());

    Catalog catalog;
    QString error;
    QVERIFY(CatalogXml::load(path(QStringLiteral("de-amateurfunk-N.xml")), &catalog, &error));

    QCOMPARE(manifest.driftOf(catalog, catalogDirectory()), CatalogDrift::AsShipped);

    const CatalogManifestEntry entry = manifest.entries().first();
    const QString text = CatalogManifest::describe(CatalogDrift::AsShipped, catalog, entry);
    QVERIFY2(text.contains(entry.catalogVersion), qPrintable(text));
}

void CatalogManifestTest::aHandChangedCatalogIsRecognisedAsEdited()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QFile::copy(path(QStringLiteral("de-amateurfunk-N.xml")),
                        dir.filePath(QStringLiteral("de-amateurfunk-N.xml"))));
    QVERIFY(QFile::copy(path(QStringLiteral("catalog-manifest.json")),
                        dir.filePath(QStringLiteral("catalog-manifest.json"))));

    // Der Sollstand wird nicht angefasst: er beschreibt den ausgelieferten
    // Stand, nicht die Datei auf der Platte.
    const CatalogManifest manifest = CatalogManifest::load(dir.path());
    QVERIFY(!manifest.isEmpty());

    Catalog catalog;
    QString error;
    QVERIFY(CatalogXml::load(dir.filePath(QStringLiteral("de-amateurfunk-N.xml")), &catalog, &error));

    // Die Erkennung haengt an der Datei auf der Platte, nicht am Objekt im
    // Speicher: wer die Katalogdatei anfasst, gilt als geaendert, egal ob das
    // Programm die Aenderung je sieht.
    QFile handle(dir.filePath(QStringLiteral("de-amateurfunk-N.xml")));
    QVERIFY(handle.open(QIODevice::ReadOnly | QIODevice::Append));
    handle.write("\n<!-- von Hand angehaengt -->\n");
    handle.close();

    Catalog reloaded;
    QVERIFY(CatalogXml::load(dir.filePath(QStringLiteral("de-amateurfunk-N.xml")), &reloaded, &error));
    QCOMPARE(manifest.driftOf(reloaded, dir.path()), CatalogDrift::Edited);

    const CatalogManifestEntry entry = manifest.entries().first();
    const QString text = CatalogManifest::describe(CatalogDrift::Edited, reloaded, entry);
    QVERIFY2(text.contains(entry.catalogVersion), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("überschrieben")), qPrintable(text));
}

void CatalogManifestTest::theDescriptionNamesTheEdition()
{
    CatalogManifestEntry entry;
    entry.catalogVersion = QStringLiteral("3. Auflage, März 2024");
    entry.validFrom = QDate(2024, 6, 24);
    entry.questions = 571;

    Catalog catalog;
    catalog.metadata.examClass = ExamClass::Novize;
    const QString text = CatalogManifest::describe(CatalogDrift::AsShipped, catalog, entry);

    QVERIFY2(text.contains(QStringLiteral("3. Auflage")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("24.06.2024")), qPrintable(text));
}

void CatalogManifestTest::loadingACatalogComputesItsDigest()
{
    Catalog catalog;
    QString error;
    QVERIFY(CatalogXml::load(path(QStringLiteral("de-amateurfunk-E.xml")), &catalog, &error));

    QFile handle(path(QStringLiteral("de-amateurfunk-E.xml")));
    QVERIFY(handle.open(QIODevice::ReadOnly));
    const QString expected =
        QString::fromLatin1(QCryptographicHash::hash(handle.readAll(), QCryptographicHash::Sha256)
                                .toHex());

    // sourceDigest bindet den Lernfortschritt an den Katalogeinstand. Ohne
    // Wert bleibt er leer, und ein geaenderter Katalog behaelt seinen
    // Fortschritt.
    QVERIFY2(!catalog.sourceDigest.isEmpty(), "sourceDigest wird nicht gefuellt");
    QCOMPARE(catalog.sourceDigest, expected);
}

QTEST_MAIN(CatalogManifestTest)
#include "tst_catalogmanifest.moc"
