// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "ui/AnswerView.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QtTest>

using namespace hrt;

namespace {

/// Eine Frage mit vier Antworten, wie sie in jedem Katalog steht.
Question vierAntworten()
{
    Question frage;
    frage.id = QStringLiteral("f1");
    frage.text = QStringLiteral("Wie gross ist die Trägerwelle im 2-m-Band?");
    for (int i = 0; i < 4; ++i) {
        Answer antwort;
        antwort.text = QStringLiteral("Antwort %1").arg(i + 1);
        antwort.correct = (i == 1);
        frage.answers.append(antwort);
    }
    return frage;
}

QVector<int> reihenfolge(int anzahl)
{
    QVector<int> ordnung;
    for (int i = 0; i < anzahl; ++i)
        ordnung.append(i);
    return ordnung;
}

QList<QAbstractButton *> steuerung(AnswerView *sicht)
{
    QList<QAbstractButton *> gefunden;
    for (QAbstractButton *knopf : sicht->findChildren<QAbstractButton *>())
        gefunden.append(knopf);
    return gefunden;
}

} // namespace

class AnswerViewTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void everyAnswerCarriesItsLetter();
    void aChosenAnswerColoursItsWholeRow();
    void anotherAnswerTakesTheColourAway();
    void theSolutionOutranksTheChoice();
    void clickingTheTextOrTheLetterChoosesTheAnswer();

private:
    Question m_frage;
    AnswerView *m_sicht = nullptr;
};

void AnswerViewTest::init()
{
    m_frage = vierAntworten();
    delete m_sicht;
    m_sicht = new AnswerView;
    m_sicht->setQuestion(&m_frage, reihenfolge(4), QDir(QStringLiteral("/tmp")));
    m_sicht->show();
}

void AnswerViewTest::everyAnswerCarriesItsLetter()
{
    // Ohne Buchstaben muss man beim Zählen der Zeilen hilfeslos sein - und
    // bei einer Antwort, die sich ueber mehrere Zeilen erstreckt, weiss man
    // nicht einmal, welche Zeile gemeint ist.
    const QStringList briefe = m_sicht->badges();
    QCOMPARE(briefe.size(), 4);
    QCOMPARE(briefe, QStringList({QStringLiteral("A"), QStringLiteral("B"),
                                 QStringLiteral("C"), QStringLiteral("D")}));
}

void AnswerViewTest::aChosenAnswerColoursItsWholeRow()
{
    const QList<QAbstractButton *> knoepfe = steuerung(m_sicht);
    QCOMPARE(knoepfe.size(), 4);

    // Vor der Antwort traegt keine Zeile Farbe. Eine Zeile, die schon vorher
    // gefaerbt waere, wuerde eine Auswahl vortaeuschen.
    for (QAbstractButton *knopf : knoepfe)
        QVERIFY(!knopf->parentWidget()->autoFillBackground());

    knoepfe.at(2)->click();

    // Die ganze Zeile, nicht nur der Punkt links: bei einer mehrzeiligen
    // Antwort ist der Punkt winzig, bei einer Bildantwort fehlt er fast.
    QWidget *gewaehlt = knoepfe.at(2)->parentWidget();
    QVERIFY2(gewaehlt->autoFillBackground(), "die gewaehlte Zeile blieb farblos");
    const QColor farbe = gewaehlt->palette().color(QPalette::Window);
    QVERIFY(farbe.blue() > farbe.red());
    QVERIFY(farbe.blue() > farbe.green() - 20);

    // Die uebrigen Zeilen bleiben unveraendert - sonst faellt die Auswahl nicht
    // mehr auf, sie faellt nur noch auf, weil alles leuchtet.
    for (int i = 0; i < 4; ++i) {
        if (i == 2)
            continue;
        QVERIFY(!knoepfe.at(i)->parentWidget()->autoFillBackground());
    }
}

void AnswerViewTest::anotherAnswerTakesTheColourAway()
{
    const QList<QAbstractButton *> knoepfe = steuerung(m_sicht);
    knoepfe.at(2)->click();
    knoepfe.at(0)->click();

    // Bleibt die erste Zeile gefaerbt, sieht die Frage aus, als waere zweimal
    // gewaehlt worden - bei einer Auswahl mit einem Klick.
    QVERIFY(knoepfe.at(0)->parentWidget()->autoFillBackground());
    QVERIFY(!knoepfe.at(2)->parentWidget()->autoFillBackground());
}

void AnswerViewTest::theSolutionOutranksTheChoice()
{
    const QList<QAbstractButton *> knoepfe = steuerung(m_sicht);
    knoepfe.at(1)->click();
    QVERIFY(knoepfe.at(1)->parentWidget()->autoFillBackground());

    // Die Loesung entscheidet: ab jetzt zaehlt nur noch, ob es stimmt. Die
    // blaeue Auswahlfarbe bliebe sonst unter der gruenen Zeile stehen und
    // wuerde den ganzen Spalt einfaerben.
    m_sicht->reveal(1, 1);

    QWidget *zeile = knoepfe.at(1)->parentWidget();
    QVERIFY(zeile->autoFillBackground());
    const QColor farbe = zeile->palette().color(QPalette::Window);
    QVERIFY2(farbe.green() > farbe.blue(), "die Loesungsfarbe kam nicht durch");
}

void AnswerViewTest::clickingTheTextOrTheLetterChoosesTheAnswer()
{
    // Den Auswahlpunkt exakt zu treffen ist muehsam. Ein Klick auf den Text
    // oder den Buchstaben der Antwort muss genauso zaehlen. Der Klick geht
    // ueber das Fenster, damit Qt wie bei einer echten Maus selbst bestimmt,
    // welches Widget getroffen ist.
    QVERIFY(QTest::qWaitForWindowExposed(m_sicht));
    const QList<QAbstractButton *> knoepfe = steuerung(m_sicht);
    QCOMPARE(knoepfe.size(), 4);

    const auto klickAuf = [this](QWidget *ziel) {
        const QPoint mitte = ziel->mapTo(m_sicht->window(), ziel->rect().center());
        QTest::mouseClick(m_sicht->window()->windowHandle(), Qt::LeftButton, {}, mitte);
    };
    const auto textDerZeile = [](QAbstractButton *knopf) -> QLabel * {
        for (QLabel *label : knopf->parentWidget()->findChildren<QLabel *>()) {
            if (label->wordWrap())
                return label;
        }
        return nullptr;
    };
    const auto buchstabeDerZeile = [](QAbstractButton *knopf) -> QLabel * {
        for (QLabel *label : knopf->parentWidget()->findChildren<QLabel *>()) {
            if (label->text().size() == 1 && label->isVisible())
                return label;
        }
        return nullptr;
    };

    QLabel *text = textDerZeile(knoepfe.at(1));
    QVERIFY(text);
    klickAuf(text);
    QCOMPARE(m_sicht->selection(), 1);
    QVERIFY(knoepfe.at(1)->isChecked());

    QLabel *buchstabe = buchstabeDerZeile(knoepfe.at(3));
    QVERIFY(buchstabe);
    QCOMPARE(buchstabe->text(), QStringLiteral("D"));
    klickAuf(buchstabe);
    QCOMPARE(m_sicht->selection(), 3);

    // Nach der Aufloesung nimmt keine Zeile mehr einen Klick an.
    m_sicht->reveal(0, 3);
    klickAuf(textDerZeile(knoepfe.at(0)));
    QCOMPARE(m_sicht->selection(), 3);
}

QTEST_MAIN(AnswerViewTest)
#include "tst_answerview.moc"
