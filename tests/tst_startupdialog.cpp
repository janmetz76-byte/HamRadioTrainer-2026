// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "StartupDialog.h"

#include "core/CatalogXml.h"

#include "ExamWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QtTest>

using namespace hrt;

namespace {

QString catalogPath(const char *file)
{
    return QStringLiteral(HRT_SOURCE_DIR "/data/catalogs/") + QString::fromLatin1(file);
}

QStringList allCatalogs()
{
    return {catalogPath("de-amateurfunk-N.xml"), catalogPath("de-amateurfunk-E.xml"),
            catalogPath("de-amateurfunk-A.xml")};
}

} // namespace

class StartupDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void partsFollowTheAmtsblatt();
    void labelsSpellThePartsOut();
    void catalogNOffersNoAscent();
    void everyShippedCatalogIsOffered();
    void classIsPreselected();
    void acceptingStoresTheChoice();
    void noSelectionMeansNoAccept();
};

void StartupDialogTest::partsFollowTheAmtsblatt()
{
    // Abschnitt 7.3 der Amtsblatt-Verfuegung 29/2024. Vollstaendig:
    QCOMPARE(StartupDialog::partsForClass(QStringLiteral("N")), QVector<int>({0, 1, 2}));
    QCOMPARE(StartupDialog::partsForClass(QStringLiteral("E")), QVector<int>({0, 1, 2, 3}));
    QCOMPARE(StartupDialog::partsForClass(QStringLiteral("A")),
             QVector<int>({0, 1, 2, 3, 4}));

    const QVector<StartupDialog::Combination> all = StartupDialog::combinations();
    QCOMPARE(all.size(), 6);

    // Wer die Klasse N hat, muss fuer E nur noch die Technik Klasse E
    // beantworten. Frueher waren nur die drei vollstaendigen Pruefungen
    // angeboten, wodurch der Assistent mehr verlangte als die Verfuegung.
    QCOMPARE(all.at(3).label, QStringLiteral("Klasse N → E"));
    QCOMPARE(all.at(3).targetClass, QStringLiteral("E"));
    QCOMPARE(all.at(3).parts, QVector<int>({3}));

    QCOMPARE(all.at(4).label, QStringLiteral("Klasse N → A"));
    QCOMPARE(all.at(4).targetClass, QStringLiteral("A"));
    QCOMPARE(all.at(4).parts, QVector<int>({3, 4}));

    QCOMPARE(all.at(5).label, QStringLiteral("Klasse E → A"));
    QCOMPARE(all.at(5).targetClass, QStringLiteral("A"));
    QCOMPARE(all.at(5).parts, QVector<int>({4}));

    // Kein Aufstieg darf Teile verlangen, die die Vorstufe schon abdeckt.
    for (int i = 3; i < all.size(); ++i) {
        QVERIFY2(!all.at(i).note.isEmpty(),
                 "ein Aufstieg muss erklaeren, was schon geprueft wurde");
        QVERIFY(!all.at(i).parts.contains(0));
        QVERIFY(!all.at(i).parts.contains(1));
    }
}

void StartupDialogTest::labelsSpellThePartsOut()
{
    // "V + B + N" sagt einem Lernenden nichts. Jede Auswahl muss daher
    // ausgeschrieben sagen, welche Teile gemeint sind.
    const QVector<StartupDialog::Combination> all = StartupDialog::combinations();
    for (const StartupDialog::Combination &combination : all) {
        for (int part : combination.parts) {
            Q_UNUSED(part);
        }
        QVERIFY(!combination.label.contains(QLatin1String("V + B"))
                && !combination.label.contains(QLatin1String("Vorschriften +")));
    }
    // Der Aufstieg von E nach A besteht allein aus der Technik Klasse A.
    QCOMPARE(all.at(5).parts.size(), 1);
}

void StartupDialogTest::catalogNOffersNoAscent()
{
    // Der Katalog N enthaelt keinen Teil E; ein Aufstieg nach E oder A
    // laesst sich damit nicht anbieten.
    Catalog n;
    QString error;
    QVERIFY(CatalogXml::load(catalogPath("de-amateurfunk-N.xml"), &n, &error));
    const QVector<StartupDialog::Combination> offered =
        StartupDialog::combinationsFor(n);
    QCOMPARE(offered.size(), 1);
    QCOMPARE(offered.first().targetClass, QStringLiteral("N"));

    Catalog e;
    QVERIFY(CatalogXml::load(catalogPath("de-amateurfunk-E.xml"), &e, &error));
    const QVector<StartupDialog::Combination> offeredE =
        StartupDialog::combinationsFor(e);
    // N vollstaendig, E vollstaendig, N -> E
    QCOMPARE(offeredE.size(), 3);
    QCOMPARE(offeredE.at(2).label, QStringLiteral("Klasse N → E"));

    Catalog a;
    QVERIFY(CatalogXml::load(catalogPath("de-amateurfunk-A.xml"), &a, &error));
    QCOMPARE(StartupDialog::combinationsFor(a).size(), 6);
}

void StartupDialogTest::everyShippedCatalogIsOffered()
{
    StartupDialog dialog(allCatalogs());
    auto *box = dialog.findChild<QComboBox *>();
    QVERIFY(box);
    QCOMPARE(box->count(), 3);
}

void StartupDialogTest::classIsPreselected()
{
    StartupDialog dialog(allCatalogs());
    auto *box = dialog.findChild<QComboBox *>();
    QCOMPARE(box->currentData().toString(), catalogPath("de-amateurfunk-N.xml"));

    const QList<QCheckBox *> parts = dialog.findChildren<QCheckBox *>();
    // Katalog N kennt nur die vollstaendige N-Pruefung; Aufstiege nach E
    // oder A sind ohne den passenden Fragenpool nicht anbietbar.
    QCOMPARE(parts.size(), 1);
    QVERIFY(parts.at(0)->isChecked());
    QVERIFY(parts.at(0)->text().contains(QStringLiteral("N")));
}

void StartupDialogTest::acceptingStoresTheChoice()
{
    StartupDialog dialog(allCatalogs());
    dialog.accept();
    QVERIFY(dialog.result().accepted);
    QCOMPARE(dialog.result().selectedParts, QSet<int>({0, 1, 2}));
    QCOMPARE(dialog.result().catalogPath, catalogPath("de-amateurfunk-N.xml"));
    QVERIFY(dialog.result().examDate.isValid());
}

void StartupDialogTest::noSelectionMeansNoAccept()
{
    StartupDialog dialog(allCatalogs());
    const QList<QCheckBox *> parts = dialog.findChildren<QCheckBox *>();
    for (QCheckBox *box : parts)
        box->setChecked(false);

    dialog.accept();
    // Ohne gewaehlte Pruefung gibt es nichts zu starten.
    QVERIFY(!dialog.result().accepted);
}

QTEST_MAIN(StartupDialogTest)
#include "tst_startupdialog.moc"