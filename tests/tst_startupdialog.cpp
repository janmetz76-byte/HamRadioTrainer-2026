// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "StartupDialog.h"

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
    void everyShippedCatalogIsOffered();
    void classIsPreselected();
    void acceptingStoresTheChoice();
    void noSelectionMeansNoAccept();
};

void StartupDialogTest::partsFollowTheAmtsblatt()
{
    // Amtsblatt-Verfügung 29/2024. Eine andere Kombination gibt es nicht,
    // und genau das darf der Assistent auch nicht anbieten.
    QCOMPARE(StartupDialog::partsForClass(QStringLiteral("N")), QVector<int>({0, 1, 2}));
    QCOMPARE(StartupDialog::partsForClass(QStringLiteral("E")), QVector<int>({0, 1, 2, 3}));
    QCOMPARE(StartupDialog::partsForClass(QStringLiteral("A")), QVector<int>({0, 1, 2, 3, 4}));

    const QStringList labels = StartupDialog::combinationLabels();
    QCOMPARE(labels.size(), 3);
    QVERIFY(labels.at(0).contains(QStringLiteral("Klasse N")));
    QVERIFY(labels.at(2).contains(QStringLiteral("Klasse A")));
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
    QCOMPARE(parts.size(), 3);
    // V+B+N ist vorgewaehlt, die beiden anderen nicht.
    QVERIFY(parts.at(0)->isChecked());
    QVERIFY(!parts.at(1)->isChecked());
    QVERIFY(!parts.at(2)->isChecked());
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