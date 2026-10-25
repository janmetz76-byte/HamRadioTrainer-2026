// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "StartupDialog.h"

#include "core/CatalogXml.h"

#include <QLabel>
#include <QRadioButton>
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

QRadioButton *choice(const StartupDialog &dialog, const QString &label)
{
    for (QRadioButton *button : dialog.findChildren<QRadioButton *>()) {
        if (button->text() == label)
            return button;
    }
    return nullptr;
}

QString summaryOf(const StartupDialog &dialog)
{
    const auto *label = dialog.findChild<QLabel *>(QStringLiteral("auswahlUebersicht"));
    return label ? label->text() : QString();
}

int questionsInPart(const char *file, int part)
{
    Catalog catalog;
    QString error;
    if (!CatalogXml::load(catalogPath(file), &catalog, &error))
        qFatal("Katalog %s nicht lesbar: %s", file, qPrintable(error));
    int questions = 0;
    for (const Section &section : catalog.parts.at(part).sections)
        questions += section.activeQuestionCount();
    return questions;
}

} // namespace

class StartupDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void partsFollowTheAmtsblatt();
    void theSixCasesAreOneChoice();
    void everyCaseOpensTheCatalogOfItsClass();
    void theSummaryNamesWhatIsLearned();
    void aMissingCatalogDisablesItsCases();
    void theLastChoiceCanBePreselected();
    void withoutCatalogsNothingIsAccepted();
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

    // Aufstiege: N -> E nur E, N -> A die Teile E und A, E -> A nur A.
    QCOMPARE(all.at(3).id, QStringLiteral("N-E"));
    QCOMPARE(all.at(3).targetClass, QStringLiteral("E"));
    QCOMPARE(all.at(3).parts, QVector<int>({3}));

    QCOMPARE(all.at(4).id, QStringLiteral("N-A"));
    QCOMPARE(all.at(4).targetClass, QStringLiteral("A"));
    QCOMPARE(all.at(4).parts, QVector<int>({3, 4}));

    QCOMPARE(all.at(5).id, QStringLiteral("E-A"));
    QCOMPARE(all.at(5).targetClass, QStringLiteral("A"));
    QCOMPARE(all.at(5).parts, QVector<int>({4}));

    // Kein Aufstieg verlangt Vorschriften, Betriebstechnik oder die Technik N:
    // sie sind mit jeder bestandenen Klasse abgedeckt.
    for (int i = 3; i < all.size(); ++i) {
        QVERIFY(all.at(i).ascent);
        QVERIFY2(!all.at(i).note.isEmpty(),
                 "ein Aufstieg muss erklaeren, was schon geprueft wurde");
        QVERIFY(!all.at(i).parts.contains(0));
        QVERIFY(!all.at(i).parts.contains(1));
        QVERIFY(!all.at(i).parts.contains(2));
    }
}

void StartupDialogTest::theSixCasesAreOneChoice()
{
    // Eine Pruefung, nicht mehrere: frueher standen die Faelle als
    // Kontrollkaestchen da, und bei mehreren Haken galt still der letzte.
    StartupDialog dialog(allCatalogs());
    const QList<QRadioButton *> buttons = dialog.findChildren<QRadioButton *>();
    QCOMPARE(buttons.size(), 6);

    int checked = 0;
    for (const QRadioButton *button : buttons)
        checked += button->isChecked() ? 1 : 0;
    QCOMPARE(checked, 1);

    QRadioButton *e = choice(dialog, QStringLiteral("Klasse E"));
    QRadioButton *ascent = choice(dialog, QStringLiteral("Von Klasse E nach A"));
    QVERIFY(e && ascent);
    e->setChecked(true);
    ascent->setChecked(true);
    QVERIFY(!e->isChecked());
}

void StartupDialogTest::everyCaseOpensTheCatalogOfItsClass()
{
    // Die Klasse entscheidet ueber den Katalog; eine eigene Katalogwahl, die
    // zum Fall nicht passt, gibt es nicht mehr.
    const struct
    {
        const char *label;
        const char *catalog;
        QSet<int> parts;
    } cases[] = {
        {"Klasse N", "de-amateurfunk-N.xml", {0, 1, 2}},
        {"Klasse E", "de-amateurfunk-E.xml", {0, 1, 2, 3}},
        {"Klasse A", "de-amateurfunk-A.xml", {0, 1, 2, 3, 4}},
        {"Von Klasse N nach E", "de-amateurfunk-E.xml", {3}},
        {"Von Klasse N nach A", "de-amateurfunk-A.xml", {3, 4}},
        {"Von Klasse E nach A", "de-amateurfunk-A.xml", {4}},
    };
    for (const auto &expected : cases) {
        StartupDialog dialog(allCatalogs());
        QRadioButton *button = choice(dialog, QString::fromUtf8(expected.label));
        QVERIFY2(button, expected.label);
        QVERIFY2(button->isEnabled(), expected.label);
        button->setChecked(true);
        dialog.accept();

        const StartupDialog::Result result = dialog.result();
        QVERIFY(result.accepted);
        QCOMPARE(result.catalogPath, catalogPath(expected.catalog));
        QCOMPARE(result.selectedParts, expected.parts);
        QVERIFY(!result.combination.isEmpty());
        QVERIFY(result.examDate.isValid());
    }
}

void StartupDialogTest::theSummaryNamesWhatIsLearned()
{
    StartupDialog dialog(allCatalogs());

    // Aufstieg N -> E: nur die Technik Klasse E, mit ihrer Fragenzahl.
    choice(dialog, QStringLiteral("Von Klasse N nach E"))->setChecked(true);
    QString text = summaryOf(dialog);
    const int eQuestions = questionsInPart("de-amateurfunk-E.xml", 3);
    QVERIFY2(text.contains(QStringLiteral("Technik Klasse E (%1 Fragen)").arg(eQuestions)),
             qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("Vorschriften (")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("Technik Klasse N (")), qPrintable(text));

    // Klasse E von vorn: alle vier Teile, und die Summe stimmt.
    choice(dialog, QStringLiteral("Klasse E"))->setChecked(true);
    text = summaryOf(dialog);
    int total = 0;
    for (int part = 0; part < 4; ++part)
        total += questionsInPart("de-amateurfunk-E.xml", part);
    for (const QString &name : {QStringLiteral("Vorschriften"), QStringLiteral("Betriebstechnik"),
                                QStringLiteral("Technik Klasse N"),
                                QStringLiteral("Technik Klasse E")})
        QVERIFY2(text.contains(name), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("Technik Klasse A")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("zusammen %1 Fragen").arg(total)), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("%1")), qPrintable(text));
}

void StartupDialogTest::aMissingCatalogDisablesItsCases()
{
    // Nur der Katalog N ist da: alles, was E oder A braucht, ist nicht
    // waehlbar, und die Vorauswahl faellt auf das, was geht.
    StartupDialog dialog({catalogPath("de-amateurfunk-N.xml")});
    QVERIFY(choice(dialog, QStringLiteral("Klasse N"))->isEnabled());
    QVERIFY(choice(dialog, QStringLiteral("Klasse N"))->isChecked());
    for (const QString &label : {QStringLiteral("Klasse E"), QStringLiteral("Klasse A"),
                                 QStringLiteral("Von Klasse N nach E"),
                                 QStringLiteral("Von Klasse N nach A"),
                                 QStringLiteral("Von Klasse E nach A")})
        QVERIFY2(!choice(dialog, label)->isEnabled(), qPrintable(label));
}

void StartupDialogTest::theLastChoiceCanBePreselected()
{
    StartupDialog dialog(allCatalogs());
    dialog.preselect(QStringLiteral("N-A"));
    QVERIFY(choice(dialog, QStringLiteral("Von Klasse N nach A"))->isChecked());

    // Ein unbekannter oder nicht waehlbarer Fall aendert nichts.
    dialog.preselect(QStringLiteral("gibt-es-nicht"));
    QVERIFY(choice(dialog, QStringLiteral("Von Klasse N nach A"))->isChecked());

    StartupDialog onlyN({catalogPath("de-amateurfunk-N.xml")});
    onlyN.preselect(QStringLiteral("E-A"));
    QVERIFY(choice(onlyN, QStringLiteral("Klasse N"))->isChecked());
}

void StartupDialogTest::withoutCatalogsNothingIsAccepted()
{
    StartupDialog dialog(QStringList{});
    for (const QRadioButton *button : dialog.findChildren<QRadioButton *>())
        QVERIFY(!button->isEnabled());
    dialog.accept();
    QVERIFY(!dialog.result().accepted);
    QVERIFY2(summaryOf(dialog).contains(QStringLiteral("kein Fragenkatalog")),
             qPrintable(summaryOf(dialog)));
}

QTEST_MAIN(StartupDialogTest)
#include "tst_startupdialog.moc"
