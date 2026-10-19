// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Prueft den Importweg, den die Menueoption "Fragenkatalog aktualisieren..."
// benutzt: Quellverzeichnis erkennen, importieren, Sollstand schreiben und
// verwaiste Bilder entfernen - ohne die Zieldateien bei einem Fehler zu
// zerstoeren.
#include <QtTest>

#include "core/Catalog.h"
#include "core/CatalogImporter.h"
#include "core/CatalogManifest.h"
#include "core/CatalogXml.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace hrt;

namespace {

/// Eine Frage in der Form der Quelle: Nummer, Klasse, Text, vier Antworten.
QJsonObject question(const QString &number, const QString &klass,
                     const QString &picture = QString())
{
    QJsonObject object;
    object.insert(QStringLiteral("number"), number);
    object.insert(QStringLiteral("class"), klass);
    object.insert(QStringLiteral("question"), QStringLiteral("Was gilt für %1?").arg(number));
    object.insert(QStringLiteral("answer_a"), QStringLiteral("Antwort A zu %1").arg(number));
    object.insert(QStringLiteral("answer_b"), QStringLiteral("Antwort B zu %1").arg(number));
    object.insert(QStringLiteral("answer_c"), QStringLiteral("Antwort C zu %1").arg(number));
    object.insert(QStringLiteral("answer_d"), QStringLiteral("Antwort D zu %1").arg(number));
    if (!picture.isEmpty())
        object.insert(QStringLiteral("picture_question"), picture);
    return object;
}

/// Baut eine Quelle im Format der Bundesnetzagentur.
///
/// Die Struktur ist an der mitgelieferten Datei abgeschrieben: fuenf
/// Blaetter unter "sections", von denen "Kenntnisse von Vorschriften" und
/// "Betriebliche Kenntnisse" ohne Klassenfilter uebernommen werden, waehrend
/// "Technische Kenntnisse" nach der Spalte "class" aufgeteilt wird. Dazu
/// "metadata" und unter "svgs/" die Bilder ohne Endung.
class SourceBuilder
{
public:
    explicit SourceBuilder(const QString &root) : m_root(root)
    {
        QDir().mkpath(m_root + QStringLiteral("/svgs"));
    }

    /// Legt ein Bild unter svgs/ ab. Die Quelle nennt sie ohne Endung.
    void addImage(const QString &name)
    {
        QFile file(m_root + QStringLiteral("/svgs/") + name + QStringLiteral(".svg"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<svg xmlns=\"http://www.w3.org/2000/svg\"/>");
    }

    /// Ein Blatt, das ungefiltert in jede Klasse uebernommen wird.
    void addSharedSheet(char letter, const QJsonArray &questions)
    {
        m_sheets.append(Blatt{letter, questions});
    }

    /// Die technischen Fragen einer Klasse. Alle Klassen teilen sich ein
    /// Blatt; getrennt werden sie ueber die Spalte "class".
    void addTechnicalSheet(const QJsonObject &question)
    {
        m_technical.questions.append(question);
    }

    void write()
    {
        QJsonArray sections;
        for (const Blatt &blatt : m_sheets) {
            QJsonObject inner;
            inner.insert(QStringLiteral("title"), QStringLiteral("Grundlagen"));
            inner.insert(QStringLiteral("questions"), blatt.questions);

            QJsonObject sheet;
            sheet.insert(QStringLiteral("title"), blattTitle(blatt.letter));
            sheet.insert(QStringLiteral("sections"), QJsonArray{inner});
            sections.append(wrap(sheet));
        }

        // Das technische Blatt sammelt alle Klassen unter sich; der Import
        // filtert sie ueber die Spalte "class". Es traegt dieselbe Doppelung
        // aus Thema und Abschnitt wie in der Quelle.
        if (!m_technical.questions.isEmpty()) {
            QJsonObject inner;
            inner.insert(QStringLiteral("title"), QStringLiteral("Technische Grundlagen"));
            inner.insert(QStringLiteral("questions"), m_technical.questions);

            QJsonObject mid;
            mid.insert(QStringLiteral("title"), QStringLiteral("Technische Kenntnisse"));
            mid.insert(QStringLiteral("sections"), QJsonArray{inner});

            QJsonObject sheet;
            sheet.insert(QStringLiteral("title"),
                         QStringLiteral("Prüfungsfragen im Prüfungsteil: Technische "
                                        "Kenntnisse"));
            sheet.insert(QStringLiteral("sections"), QJsonArray{mid});
            sections.append(wrap(sheet));
        }

        QJsonObject metadata;
        metadata.insert(QStringLiteral("edition"), QStringLiteral("4. Auflage, Januar 2028"));
        metadata.insert(QStringLiteral("issued_on"), QStringLiteral("2028-01-05"));
        metadata.insert(QStringLiteral("valid_from"), QStringLiteral("2028-03-01"));
        metadata.insert(QStringLiteral("license"), QStringLiteral("DL-DE-BY-2.0"));

        QJsonObject root;
        root.insert(QStringLiteral("sections"), sections);
        root.insert(QStringLiteral("metadata"), metadata);

        QFile file(m_root + QStringLiteral("/fragenkatalog3b.json"));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    QString path() const { return m_root; }

private:
    struct Blatt
    {
        char letter = 0;
        QJsonArray questions;
    };

    static QString blattTitle(char letter)
    {
        switch (letter) {
        case 'V':
            return QStringLiteral("Prüfungsfragen im Prüfungsteil: Kenntnisse von "
                                  "Vorschriften");
        case 'B':
            return QStringLiteral("Prüfungsfragen im Prüfungsteil: Betriebliche "
                                  "Kenntnisse");
        default:
            return QStringLiteral("Prüfungsfragen im Prüfungsteil: Technische Kenntnisse");
        }
    }

    /// Die Quelle bettet jedes Blatt noch einmal in "sections" ein.
    static QJsonObject wrap(const QJsonObject &sheet)
    {
        QJsonObject outer;
        outer.insert(QStringLiteral("title"), sheet.value(QStringLiteral("title")));
        outer.insert(QStringLiteral("sections"), QJsonArray{sheet});
        return outer;
    }

    QString m_root;
    QVector<Blatt> m_sheets;
    Blatt m_technical;
};

/// Erzeugt genug Fragen fuer einen ganzen Pruefungsteil.
///
/// Der Umfang ist nicht frei gewaehlt: nach der Amtsblatt-Verfuegung 29/2024
/// besteht jeder Pruefungsteil aus 25 Fragen, und der Validator weist einen
/// Katalog mit weniger als Fehler zurueck. Eine Testquelle mit drei Fragen
/// waere also ein Katalog, den das Programm gar nicht benutzen darf.
QJsonArray blatt(const QString &vorsatz, const QString &klass)
{
    QJsonArray Fragen;
    for (int i = 1; i <= 25; ++i)
        Fragen.append(question(QStringLiteral("%1%2").arg(vorsatz).arg(i, 2, 10, QLatin1Char('0')),
                               klass));
    return Fragen;
}

/// Wie blatt(), aber die erste Frage traegt ein Bild.
QJsonArray bildblatt(const QString &vorsatz, const QString &klass, const QString &bild)
{
    QJsonArray fragen = blatt(vorsatz, klass);
    fragen[0] = question(QStringLiteral("%1%2").arg(vorsatz).arg(1, 2, 10, QLatin1Char('0')),
                         klass, bild);
    return fragen;
}

/// Fuellt eine Quelle mit allem, was eine brauchbare Quelle braucht: die
/// beiden geteilten Blaetter und die technischen Fragen je Klasse.
void befuellen(SourceBuilder &source, const QJsonArray &klassN = QJsonArray{},
               const QJsonArray &klassE = QJsonArray{}, const QJsonArray &klassA = QJsonArray{})
{
    source.addSharedSheet('V', blatt(QStringLiteral("VB"), QStringLiteral("1")));
    source.addSharedSheet('B', blatt(QStringLiteral("BB"), QStringLiteral("1")));
    for (const QJsonValue &value : klassN)
        source.addTechnicalSheet(value.toObject());
    for (const QJsonValue &value : klassE)
        source.addTechnicalSheet(value.toObject());
    for (const QJsonValue &value : klassA)
        source.addTechnicalSheet(value.toObject());
    source.write();
}

} // namespace

class CatalogImporterTest : public QObject
{
    Q_OBJECT

private slots:
    void sourceIsUsableNamesTheMissingFile();
    void aDirectoryWithoutTheArchiveIsRejected();
    void anUnreadableArchiveIsRejected();
    void anArchiveWithoutMetadataIsRejected();
    void anImportWritesAllClassesAndTheirManifest();
    void anImportCopiesTheImagesTheQuestionsName();
    void anImportRemovesImagesNoQuestionUsesAnymore();
    void aRejectedArchiveLeavesThePreviousCatalogsInPlace();
};

void CatalogImporterTest::sourceIsUsableNamesTheMissingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString error;
    QVERIFY2(!CatalogImporter::sourceIsUsable(dir.path(), &error),
             "ein leeres Verzeichnis muss abgelehnt werden");
    QVERIFY2(error.contains(QStringLiteral("fragenkatalog3b.json")), qPrintable(error));
}

void CatalogImporterTest::aDirectoryWithoutTheArchiveIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir().mkpath(dir.path() + QStringLiteral("/svgs"));

    QString error;
    QVERIFY(!CatalogImporter::sourceIsUsable(dir.path(), &error));
    QVERIFY(error.contains(QStringLiteral("fragenkatalog3b.json")));
}

void CatalogImporterTest::anUnreadableArchiveIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.path() + QStringLiteral("/fragenkatalog3b.json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{ das ist kein JSON");

    QString error;
    QVERIFY2(!CatalogImporter::sourceIsUsable(dir.path(), &error),
             "kaputtes JSON muss abgelehnt werden");
    QVERIFY2(!error.isEmpty(), "der Fehler muss sagen, was fehlt");
}

void CatalogImporterTest::anArchiveWithoutMetadataIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile file(dir.path() + QStringLiteral("/fragenkatalog3b.json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"sections\": []}");

    QString error;
    QVERIFY2(!CatalogImporter::sourceIsUsable(dir.path(), &error),
             "ohne metadata ist das nicht das Archiv der Bundesnetzagentur");
    QVERIFY2(error.contains(QStringLiteral("metadata")), qPrintable(error));
}

void CatalogImporterTest::anImportWritesAllClassesAndTheirManifest()
{
    QTemporaryDir quelle;
    QTemporaryDir ziel;
    QVERIFY(quelle.isValid());
    QVERIFY(ziel.isValid());

    SourceBuilder source(quelle.path());
    befuellen(source,
              blatt(QStringLiteral("NA"), QStringLiteral("1")),
              blatt(QStringLiteral("EA"), QStringLiteral("2")),
              blatt(QStringLiteral("AA"), QStringLiteral("3")));

    const CatalogImportReport report =
        CatalogImporter::importSource(source.path(), ziel.path());
    QVERIFY2(report.ok, qPrintable(report.errors.join(QLatin1Char('\n'))));
    QCOMPARE(report.errors.size(), 0);
    QCOMPARE(report.catalogs.size(), 3);

    for (const CatalogImportEntry &entry : report.catalogs) {
        QVERIFY(QFile::exists(entry.path));
        // Kenntnisse von Vorschriften und Betriebliche Kenntnisse - je 25
        // Fragen - plus die technischen Fragen bis zur eigenen Klasse. Klasse
        // N besteht damit aus 25+25+25, Klasse A aus 25*5.
        const int erwartet = entry.fileName.endsWith(QStringLiteral("-N.xml"))   ? 75
                             : entry.fileName.endsWith(QStringLiteral("-E.xml")) ? 100
                                                                                 : 125;
        QCOMPARE(entry.questions, erwartet);
        QVERIFY2(entry.previousContent.isEmpty(), "es gab keine Vorgabedatei");
    }

    // Der Sollstand gehoert zum Import: ohne ihn koennte das Programm einen
    // frisch aktualisierten Katalog nicht wiedererkennen.
    const CatalogManifest manifest = CatalogManifest::load(ziel.path());
    QVERIFY2(!manifest.isEmpty(), "der Import muss den Sollstand schreiben");
    QCOMPARE(manifest.entries().size(), 3);

    Catalog catalog;
    QString error;
    QVERIFY(CatalogXml::load(ziel.path() + QStringLiteral("/de-amateurfunk-N.xml"),
                             &catalog, &error));
    QCOMPARE(manifest.driftOf(catalog, ziel.path()), CatalogDrift::AsShipped);
}

void CatalogImporterTest::anImportCopiesTheImagesTheQuestionsName()
{
    QTemporaryDir quelle;
    QTemporaryDir ziel;
    QVERIFY(quelle.isValid());
    QVERIFY(ziel.isValid());

    SourceBuilder source(quelle.path());
    source.addImage(QStringLiteral("AA01_q"));
    befuellen(source,
              blatt(QStringLiteral("NA"), QStringLiteral("1")),
              blatt(QStringLiteral("EA"), QStringLiteral("2")),
              bildblatt(QStringLiteral("AA"), QStringLiteral("3"),
                        QStringLiteral("AA01_q")));

    const CatalogImportReport report = CatalogImporter::importSource(source.path(), ziel.path());
    QVERIFY2(report.ok, qPrintable(report.errors.join(QLatin1Char('\n'))));
    QCOMPARE(report.imagesCopied, 1);
    QCOMPARE(report.imagesMissing, 0);

    QVERIFY(QFile::exists(ziel.path() + QStringLiteral("/images/AA01_q.svg")));

    // Der Bildname im Katalog zeigt auf images/, nicht auf svgs/.
    Catalog catalog;
    QString error;
    QVERIFY(CatalogXml::load(ziel.path() + QStringLiteral("/de-amateurfunk-A.xml"),
                             &catalog, &error));

    QString gefunden;
    for (const Question *frage : catalog.questions()) {
        if (!frage->image.isEmpty())
            gefunden = frage->image;
    }
    QCOMPARE(gefunden, QStringLiteral("images/AA01_q.svg"));
}

void CatalogImporterTest::anImportRemovesImagesNoQuestionUsesAnymore()
{
    QTemporaryDir quelle;
    QTemporaryDir ziel;
    QVERIFY(quelle.isValid());
    QVERIFY(ziel.isValid());

    SourceBuilder source(quelle.path());
    source.addImage(QStringLiteral("AA01_q"));
    source.addImage(QStringLiteral("ALT_q"));
    befuellen(source,
              blatt(QStringLiteral("NA"), QStringLiteral("1")),
              blatt(QStringLiteral("EA"), QStringLiteral("2")),
              bildblatt(QStringLiteral("AA"), QStringLiteral("3"),
                        QStringLiteral("AA01_q")));

    QVERIFY(QDir().mkpath(ziel.path() + QStringLiteral("/images")));
    QVERIFY(QFile::copy(quelle.path() + QStringLiteral("/svgs/ALT_q.svg"),
                        ziel.path() + QStringLiteral("/images/ALT_q.svg")));

    const CatalogImportReport report = CatalogImporter::importSource(source.path(), ziel.path());
    QVERIFY2(report.ok, qPrintable(report.errors.join(QLatin1Char('\n'))));

    // Ein Bild, das keine Frage mehr nennt, wird entfernt. Bleibt es liegen,
    // waechst das Verzeichnis bei jedem Import, und es bleibt offen, was
    // dazugehoert.
    QVERIFY(QFile::exists(ziel.path() + QStringLiteral("/images/AA01_q.svg")));
    QVERIFY2(!QFile::exists(ziel.path() + QStringLiteral("/images/ALT_q.svg")),
             "verwaistes Bild muss weg");
}

void CatalogImporterTest::aRejectedArchiveLeavesThePreviousCatalogsInPlace()
{
    QTemporaryDir quelle;
    QTemporaryDir ziel;
    QVERIFY(quelle.isValid());
    QVERIFY(ziel.isValid());

    // Erst ein guter Import, damit es etwas zu zerstoeren gibt.
    {
        SourceBuilder source(quelle.path());
        befuellen(source,
                  blatt(QStringLiteral("NA"), QStringLiteral("1")),
                  blatt(QStringLiteral("EA"), QStringLiteral("2")),
                  blatt(QStringLiteral("AA"), QStringLiteral("3")));
        const CatalogImportReport report =
            CatalogImporter::importSource(source.path(), ziel.path());
        QVERIFY2(report.ok, qPrintable(report.errors.join(QLatin1Char('\n'))));
    }

    QFile vorher(ziel.path() + QStringLiteral("/de-amateurfunk-N.xml"));
    QVERIFY(vorher.open(QIODevice::ReadOnly));
    const QByteArray stand = vorher.readAll();
    vorher.close();

    // Dann eine Quelle, die zwar parsebar ist, aber keine Klassen liefert.
    SourceBuilder kaputt(quelle.path());
    kaputt.write();

    const CatalogImportReport report = CatalogImporter::importSource(quelle.path(), ziel.path());
    QVERIFY2(!report.ok, "eine Quelle ohne brauchbare Klassen darf nicht als Erfolg gelten");
    QVERIFY(!report.errors.isEmpty());

    // Der vorherige Stand bleibt lesbar. Ein abgebrochener Import darf keinen
    // Katalog hinterlassen, mit dem niemand mehr lernen kann.
    QFile nachher(ziel.path() + QStringLiteral("/de-amateurfunk-N.xml"));
    QVERIFY(nachher.open(QIODevice::ReadOnly));
    QCOMPARE(nachher.readAll(), stand);
}

QTEST_MAIN(CatalogImporterTest)
#include "tst_catalogimporter.moc"
