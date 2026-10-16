// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "CatalogView.h"

#include "core/CatalogXml.h"

#include <QHeaderView>
#include <QLabel>
#include <QSettings>
#include <QTableWidget>
#include <QTreeWidget>
#include <QtTest>

using namespace hrt;

namespace {

Catalog &loadCatalog()
{
    static Catalog catalog;
    static bool loaded = false;
    if (!loaded) {
        QString error;
        if (!CatalogXml::load(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs/de-amateurfunk-N.xml"),
                              &catalog, &error))
            qFatal("Katalog nicht lesbar: %s", qPrintable(error));
        loaded = true;
    }
    return catalog;
}

QString catalogPath()
{
    return QStringLiteral(HRT_SOURCE_DIR "/data/catalogs/de-amateurfunk-N.xml");
}

/// The tree row of the chapter that holds the given question.
///
/// The tree carries no question items, so the chapter has to be looked up in
/// the catalog first and then matched by its title in the tree.
QTreeWidgetItem *chapterOf(QTreeWidget *tree, const QString &questionId, const Catalog &catalog)
{
    QString wanted;
    for (const Part &part : catalog.parts) {
        for (const Section &section : part.sections) {
            for (const Chapter &chapter : section.chapters) {
                for (const Question &question : chapter.questions) {
                    if (question.id == questionId) {
                        wanted = chapter.title;
                        break;
                    }
                }
                if (!wanted.isEmpty())
                    break;
            }
            if (!wanted.isEmpty())
                break;
        }
        if (!wanted.isEmpty())
            break;
    }
    if (wanted.isEmpty())
        return nullptr;

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *part = tree->topLevelItem(i);
        for (int j = 0; j < part->childCount(); ++j) {
            QTreeWidgetItem *section = part->child(j);
            for (int k = 0; k < section->childCount(); ++k) {
                if (section->child(k)->text(0) == wanted)
                    return section->child(k);
            }
        }
    }
    return nullptr;
}

} // namespace

class CatalogViewTest : public QObject
{
    Q_OBJECT

private slots:
    void treeCarriesTheQuestionCount();
    void selectingAChapterFillsTheQuestionList();
    void listUsesTheColumnsOfThe2015Trainer();
    void progressReachesTheList();
    void aQuestionWithAnImageShowsIt();
    void columnWidthsCanBeDraggedAndSurviveARestart();
    void partTitlesCarryNoRegulation();
};

void CatalogViewTest::treeCarriesTheQuestionCount()
{
    CatalogView view;
    view.setCatalog(&loadCatalog(), catalogPath());

    QTreeWidget *tree = view.findChild<QTreeWidget *>();
    QVERIFY(tree);
    QVERIFY(tree->topLevelItemCount() > 0);

    // Ohne Zahl ist die Spalte sinnlos - beim Einfuehren der Ansicht war sie
    // zuerst leer, weil die Werte in die interne Zuordnung geschrieben wurden.
    const QTreeWidgetItem *first = tree->topLevelItem(0);
    QVERIFY2(first->text(1).toInt() > 0,
             qPrintable(QStringLiteral("Prüfungsteil ohne Fragezahl: '%1'").arg(first->text(1))));
}

void CatalogViewTest::selectingAChapterFillsTheQuestionList()
{
    CatalogView view;
    view.setCatalog(&loadCatalog(), catalogPath());
    QTreeWidget *tree = view.findChild<QTreeWidget *>();
    QTableWidget *table = view.findChild<QTableWidget *>();
    QVERIFY(tree && table);

    tree->setCurrentItem(tree->topLevelItem(0));
    QVERIFY(table->rowCount() > 0);
    QVERIFY(table->rowCount() <= loadCatalog().totalQuestionCount());
}

void CatalogViewTest::listUsesTheColumnsOfThe2015Trainer()
{
    CatalogView view;
    view.setCatalog(&loadCatalog(), catalogPath());
    QTableWidget *table = view.findChild<QTableWidget *>();
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 5);

    const QStringList headers = {table->horizontalHeaderItem(0)->text(),
                                 table->horizontalHeaderItem(1)->text(),
                                 table->horizontalHeaderItem(2)->text(),
                                 table->horizontalHeaderItem(3)->text(),
                                 table->horizontalHeaderItem(4)->text()};
    QCOMPARE(headers, QStringList({QStringLiteral("Kennung"), QStringLiteral("Frage"),
                                  QStringLiteral("Lernstufe"), QStringLiteral("Letzte Abfrage"),
                                  QStringLiteral("Wiedervorlage")}));
}

void CatalogViewTest::progressReachesTheList()
{
    CatalogView view;
    view.setCatalog(&loadCatalog(), catalogPath());

    QVector<QuestionState> states;
    QuestionState state;
    state.questionId = QStringLiteral("VA101");
    state.timesAnswered = 4;
    state.timesCorrect = 3;
    state.box = 3;
    state.lastSeen = QDate(2026, 10, 1);
    state.dueOn = QDate(2026, 10, 8);
    states.append(state);
    view.setProgress(states);

    QTreeWidget *tree = view.findChild<QTreeWidget *>();
    QTableWidget *table = view.findChild<QTableWidget *>();
    tree->setCurrentItem(tree->topLevelItem(0));
    QVERIFY(table->rowCount() > 0);

    int row = -1;
    for (int r = 0; r < table->rowCount(); ++r) {
        if (table->item(r, 0)->text() == QStringLiteral("VA101"))
            row = r;
    }
    QVERIFY2(row >= 0, "VA101 nicht in der Liste");

    // Die Lernstufe und die Wiedervorlage sind der Grund, warum es diese
    // Spalten ueberhaupt gibt.
    QVERIFY(table->item(row, 2)->text().contains(QStringLiteral("Stufe 3")));
    QCOMPARE(table->item(row, 3)->text(), QStringLiteral("01.10.2026"));
    QCOMPARE(table->item(row, 4)->text(), QStringLiteral("08.10.2026"));
}

void CatalogViewTest::aQuestionWithAnImageShowsIt()
{
    // NI104 hat vier Antworten, die nur aus einem Schaltbild bestehen.
    CatalogView view;
    view.resize(1400, 900);
    view.setCatalog(&loadCatalog(), catalogPath());
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QTreeWidget *tree = view.findChild<QTreeWidget *>();
    QTableWidget *table = view.findChild<QTableWidget *>();
    QTreeWidgetItem *chapter = chapterOf(tree, QStringLiteral("NI104"), loadCatalog());
    QVERIFY2(chapter, "Kapitel mit NI104 nicht gefunden");

    tree->setCurrentItem(chapter);
    int row = -1;
    for (int r = 0; r < table->rowCount(); ++r) {
        if (table->item(r, 0)->text() == QStringLiteral("NI104"))
            row = r;
    }
    QVERIFY2(row >= 0, "NI104 nicht in der Fragenliste");

    table->selectRow(row);
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    // Das Bild muss in der Detailansicht als echtes Bild erscheinen, nicht
    // als Textverweis.
    int pixmapLabels = 0;
    for (QLabel *label : view.findChildren<QLabel *>()) {
        if (!label->pixmap().isNull())
            ++pixmapLabels;
    }
    QVERIFY2(pixmapLabels >= 4,
             qPrintable(QStringLiteral("nur %1 Bildlabels, erwartet 4 Antwortbilder")
                            .arg(pixmapLabels)));
}

void CatalogViewTest::columnWidthsCanBeDraggedAndSurviveARestart()
{
    // Ausgangslage ohne gemerkte Groessen, damit der Test nicht von der
    // Reihenfolge der uebrigen Faelle abhaengt.
    QSettings().remove(QStringLiteral("catalogView"));

    const int draggedWidth = 260;
    int widthAfterDrag = 0;
    {
        CatalogView view;
        view.setCatalog(&loadCatalog(), catalogPath());
        QTableWidget *table = view.findChild<QTableWidget *>(QStringLiteral("fragenliste"));
        QVERIFY(table);
        QTreeWidget *tree = view.findChild<QTreeWidget *>(QStringLiteral("kapitelbaum"));
        QVERIFY(tree);

        // Nur eine interaktive Spalte laesst die Maus ueberhaupt an der
        // Grenze ziehen; Stretch und ResizeToContents blockieren das.
        for (int column = 0; column < table->columnCount(); ++column) {
            QCOMPARE(table->horizontalHeaderItem(column) != nullptr, true);
            QCOMPARE(table->horizontalHeader()->sectionResizeMode(column),
                     QHeaderView::ResizeMode::Interactive);
        }
        QCOMPARE(tree->header()->sectionResizeMode(0), QHeaderView::ResizeMode::Interactive);
        QCOMPARE(tree->header()->sectionResizeMode(1), QHeaderView::ResizeMode::Interactive);
        QCOMPARE(tree->header()->stretchLastSection(), false);

        table->setColumnWidth(0, draggedWidth);
        tree->setColumnWidth(0, 180);
        widthAfterDrag = table->columnWidth(0);

        // Und die Breite darf nicht beim naechsten Fuellen der Liste wieder
        // von resizeColumnsToContents() ueberschrieben werden.
        view.setProgress({});
        QCOMPARE(table->columnWidth(0), widthAfterDrag);
    }

    // Ein neues Fenster ist so ungefaehr ein Programmneustart.
    {
        CatalogView view;
        view.setCatalog(&loadCatalog(), catalogPath());
        QTableWidget *table = view.findChild<QTableWidget *>(QStringLiteral("fragenliste"));
        QTreeWidget *tree = view.findChild<QTreeWidget *>(QStringLiteral("kapitelbaum"));
        QVERIFY(table);
        QCOMPARE(table->columnWidth(0), widthAfterDrag);
        QCOMPARE(tree->columnWidth(0), 180);
    }
}

void CatalogViewTest::partTitlesCarryNoRegulation()
{
    // Die Verfuegung steht in der Hilfe, nicht an jeder Zeile der
    // Katalogansicht - sie frisst dort nur Platz.
    CatalogView view;
    view.setCatalog(&loadCatalog(), catalogPath());

    QTreeWidget *tree = view.findChild<QTreeWidget *>();
    QVERIFY(tree);
    QVERIFY(tree->topLevelItemCount() > 0);
    for (int row = 0; row < tree->topLevelItemCount(); ++row) {
        const QTreeWidgetItem *item = tree->topLevelItem(row);
        QVERIFY2(!item->text(0).contains(QStringLiteral("Amtsblatt")),
                 qPrintable(item->text(0)));
        QVERIFY2(!item->text(0).contains(QStringLiteral("29/2024")),
                 qPrintable(item->text(0)));
    }
}

QTEST_MAIN(CatalogViewTest)
#include "tst_catalogview.moc"