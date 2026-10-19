// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "StatisticsWidget.h"

#include "core/AppIdentity.h"

#include <QLabel>
#include <QProgressBar>
#include <QStandardPaths>
#include <QTableWidget>
#include <QtTest>

using namespace hrt;

namespace {

/// Ein Katalog mit zwei Teilen und je vier Fragen.
Catalog zweiTeile()
{
    Catalog katalog;
    for (const auto &teil : {std::make_pair(QStringLiteral("V"), QStringLiteral("Vorschriften")),
                             std::make_pair(QStringLiteral("N"), QStringLiteral("Technik"))}) {
        Part p;
        p.id = teil.first;
        p.title = teil.second;
        Section s;
        s.id = teil.first + QStringLiteral("-1");
        s.title = QStringLiteral("Abschnitt");
        Chapter c;
        c.id = teil.first + QStringLiteral("-1-1");
        c.title = QStringLiteral("Kapitel");
        for (int i = 0; i < 4; ++i) {
            Question frage;
            frage.id = QStringLiteral("%1-%2").arg(teil.first).arg(i);
            frage.text = QStringLiteral("Frage %1").arg(i);
            c.questions.append(frage);
        }
        s.chapters.append(c);
        p.sections.append(s);
        katalog.parts.append(p);
    }
    return katalog;
}

QLabel *findLabelStartingWith(const QWidget *wurzel, const QString &anfang)
{
    for (QLabel *label : wurzel->findChildren<QLabel *>()) {
        if (label->text().startsWith(anfang))
            return label;
    }
    return nullptr;
}

} // namespace

class StatisticsWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void withoutCatalogNothingIsWritten();
    void aFreshCatalogShowsZeroesInsteadOfNothing();
    void theProgressIsSummedUp();
    void everyPartGetsARow();

private:
    Catalog m_katalog = zweiTeile();
};

void StatisticsWidgetTest::initTestCase()
{
    hrt::setApplicationIdentity();
    QStandardPaths::setTestModeEnabled(true);
}

void StatisticsWidgetTest::withoutCatalogNothingIsWritten()
{
    StatisticsWidget w;
    w.show();

    // Ohne Katalog darf keine Zahl stehen. Eine Zeile "0 von 0 Fragen" sieht
    // nach einem Fehler aus und nicht nach einem, der noch gar nichts
    // geladen hat.
    QVERIFY(!findLabelStartingWith(&w, QStringLiteral("0 von")));
    QCOMPARE(w.findChild<QTableWidget *>(QStringLiteral("statistikTeile"))->rowCount(), 0);
}

void StatisticsWidgetTest::aFreshCatalogShowsZeroesInsteadOfNothing()
{
    StatisticsWidget w;
    w.setCatalog(&m_katalog, QStringLiteral("/tmp/statistik.xml"));
    w.show();

    // Der Katalog ist da, es wurde noch nichts beantwortet. Die Ansicht muss
    // das sagen und darf nicht leer bleiben - eine leere Ansicht haette keine
    // Frage beantwortet.
    QLabel *zeile = findLabelStartingWith(&w, QStringLiteral("0 von"));
    QVERIFY2(zeile, "die Zusammenfassung fehlt");
    QVERIFY(zeile->text().contains(QStringLiteral("8")));
    QCOMPARE(w.findChild<QTableWidget *>(QStringLiteral("statistikTeile"))->rowCount(), 2);
}

void StatisticsWidgetTest::theProgressIsSummedUp()
{
    StatisticsWidget w;
    w.setCatalog(&m_katalog, QStringLiteral("/tmp/statistik.xml"));

    QVector<QuestionState> states;
    // Zwei von vier Fragen im V-Teil gefestigt, eine nur gesehen.
    const QStringList gefestigt = {QStringLiteral("V-0"), QStringLiteral("V-1")};
    for (const QString &id : gefestigt) {
        QuestionState stand;
        stand.questionId = id;
        stand.timesAnswered = 5;
        stand.timesCorrect = 5;
        stand.box = Scheduler::MaxBox;
        states.append(stand);
    }
    {
        QuestionState stand;
        stand.questionId = QStringLiteral("V-2");
        stand.timesAnswered = 1;
        stand.timesCorrect = 0;
        stand.box = 0;
        states.append(stand);
    }
    w.setProgress(states);
    w.show();

    QLabel *zeile = findLabelStartingWith(&w, QStringLiteral("3 von"));
    QVERIFY2(zeile, qPrintable(QStringLiteral("Zusammenfassung: %1")
                                    .arg(findLabelStartingWith(&w, QString())
                                             ? findLabelStartingWith(&w, QString())->text()
                                             : QString())));
    QVERIFY(zeile->text().contains(QStringLiteral("2 gefestigt")));
    QVERIFY(zeile->text().contains(QStringLiteral("1 wackelig")));

    // Die Verteilung zeigt je Stufe eine Zahl. Ohne sie waere die Tabelle
    // eine Liste von Nullen ohne Aussage.
    QTableWidget *stufen = w.findChild<QTableWidget *>(QStringLiteral("statistikStufen"));
    QVERIFY(stufen);
    QCOMPARE(stufen->rowCount(), Scheduler::MaxBox + 1);
    QCOMPARE(stufen->item(0, 2)->text(), QStringLiteral("1"));   // wackelig
    QCOMPARE(stufen->item(Scheduler::MaxBox, 2)->text(), QStringLiteral("2"));
}

void StatisticsWidgetTest::everyPartGetsARow()
{
    StatisticsWidget w;
    w.setCatalog(&m_katalog, QStringLiteral("/tmp/statistik.xml"));

    QuestionState stand;
    stand.questionId = QStringLiteral("N-0");
    stand.timesAnswered = 4;
    stand.timesCorrect = 4;
    stand.box = Scheduler::MaxBox;
    w.setProgress({stand});
    w.show();

    QTableWidget *teile = w.findChild<QTableWidget *>(QStringLiteral("statistikTeile"));
    QVERIFY(teile);
    QCOMPARE(teile->rowCount(), 2);

    // Jeder Teil bekommt seine Zeile mit seiner Zahl. Ein Teil ohne Zeile
    // waehrte als "nichts vorhanden", nicht als "nichts gelernt".
    QString namen;
    for (int row = 0; row < teile->rowCount(); ++row)
        namen += teile->item(row, 0)->text() + QStringLiteral(" | ");
    QVERIFY(namen.contains(QStringLiteral("Vorschriften")));
    QVERIFY(namen.contains(QStringLiteral("Technik")));

    for (int row = 0; row < teile->rowCount(); ++row)
        QCOMPARE(teile->item(row, 1)->text(), QStringLiteral("4"));
    // Nur der N-Teil hat eine gefestigte Frage.
    QCOMPARE(teile->item(1, 3)->text(), QStringLiteral("1"));
}

QTEST_MAIN(StatisticsWidgetTest)
#include "tst_statisticswidget.moc"