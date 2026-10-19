// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Prueft den Icon-Satz. Ein Icon, das fehlt, ist nicht gleich harmlos: es
// fehlt genau an den Knoepfen, die eine Handlung ausloesen, und niemand merkt
// es beim Lesen des Quelltextes.
#include <QtTest>

#include "AppIcons.h"
#include "CatalogView.h"
#include "ExamWidget.h"
#include "LearningWidget.h"
#include "MainWindow.h"
#include "ValidationReport.h"

#include <QAbstractButton>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QMenuBar>
#include <QPushButton>
#include <QTextStream>
#include <QToolButton>

using namespace hrt;

class IconsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    /// Jeder Name im Satz laesst sich laden - auch die Variante fuer dunkle
    /// Fenster, die Qt nicht selbst waehlt.
    void everyNameLoadsInBothColours();
    /// Ein Icon, das nichts zeigt, ist schlimmer als keines: es sieht aus wie
    /// eine Bedienung und tut nichts.
    void everyIconDrawsSomething();
    /// Zu volle Bilder lesen sich als Block, nicht als Zeichen.
    void noIconIsAFilledBlock();
    /// Jeder Menuepunkt traegt ein Bild. Text allein ist im Menue eine
    /// Gewohnheit von 1995, keine Auszeichnung.
    void everyMenuEntryCarriesAnIcon();
    /// Die Bedienknoepfe der drei Ansichten.
    void theActionButtonsCarryIcons();
    /// Namen, die niemand benutzt, sind Ballast: sie liegen nur da und
    /// veralten, ohne dass es auffaellt.
    void noIconGoesUnused();
    /// Die Desktop-Datei sucht das Symbol unter dem Programmnamen. Steht
    /// dort ein anderer Name, zeigt der Starter ein Platzhalterbild - und das
    /// ist nicht einmal auf dem eigenen Rechner zu sehen, sondern erst beim
    ///-installierten Programm.
    void theDesktopFileFindsTheIcon();

private:
    /// Zaehlt die sichtbaren Pixel eines Bildes.
    static int gezaehlt(const QImage &bild)
    {
        int summe = 0;
        for (int y = 0; y < bild.height(); ++y) {
            for (int x = 0; x < bild.width(); ++x) {
                if (qAlpha(bild.pixel(x, y)) > 8)
                    ++summe;
            }
        }
        return summe;
    }

    QStringList m_erwartet;
};

void IconsTest::initTestCase()
{
    // Der Satz, den die Oberflaeche benutzt. Er steht hier fest, damit eine
    // Umbenennung nicht stillschweigend ein leeres Feld zuruecklaesst.
    m_erwartet = {QStringLiteral("about"),      QStringLiteral("app"),
                  QStringLiteral("assistant"),  QStringLiteral("back"),
                  QStringLiteral("collapse"),    QStringLiteral("confident"),
                  QStringLiteral("copy"),       QStringLiteral("expand"),
                  QStringLiteral("evaluate"),   QStringLiteral("guessed"),
                  QStringLiteral("next"),       QStringLiteral("open"),
                  QStringLiteral("play"),       QStringLiteral("quit"),
                  QStringLiteral("recheck"),    QStringLiteral("reload"),
                  QStringLiteral("report"),     QStringLiteral("reset-progress"),
                  QStringLiteral("skip"),       QStringLiteral("unsure"),
                  QStringLiteral("update"),     QStringLiteral("view-catalog"),
                  QStringLiteral("view-learn"), QStringLiteral("view-exam"),
                  QStringLiteral("view-statistics")};

    // Die Stufenbilder heissen nach ihrer Zahl, nicht nach einem Wort, das man
    // sich merken muesste. Der Code baut sie genauso - hier wird dieser Bau
    // nachvollzogen, sonst waeren sie fuer den Test unbenutzt, obwohl sie
    // im Programm stehen.
    for (int stufe = 0; stufe <= 5; ++stufe)
        m_erwartet.append(QStringLiteral("stufe-%1").arg(stufe));

    // Die Statussymbole des Katalogbaums kommen aus derselben Zeichenfunktion.
    for (const QString &name : {QStringLiteral("status-new"), QStringLiteral("status-low"),
                                QStringLiteral("status-mid"), QStringLiteral("status-high")})
        m_erwartet.append(name);
}

void IconsTest::everyNameLoadsInBothColours()
{
#ifdef HRT_HAVE_SVG
    for (const QString &name : m_erwartet) {
        const QIcon icon = AppIcons::get(name);
        QVERIFY2(!icon.isNull(), qPrintable(QStringLiteral("Icon fehlt: %1").arg(name)));
        for (const int seite : {16, 24, 48}) {
            const QPixmap bild = icon.pixmap(seite, seite);
            QVERIFY2(!bild.isNull(),
                     qPrintable(QStringLiteral("%1 liefert bei %2 px nichts")
                                    .arg(name)
                                    .arg(seite)));
        }
    }
#else
    QSKIP("ohne Qt6::Svg gibt es keine Iconbilder");
#endif
}

void IconsTest::everyIconDrawsSomething()
{
#ifdef HRT_HAVE_SVG
    for (const QString &name : m_erwartet) {
        const QImage bild = AppIcons::get(name).pixmap(48, 48).toImage();
        QVERIFY2(!bild.isNull(), qPrintable(name));

        // qAlpha und nicht QColor(QRgb).alpha(): die Qt-eigene Farbklasse
        // liefert hier 255 fuer ein vollstaendig durchsichtiges Pixel und
        // haelt damit jedes Bild fuer gezeichnet - der Test waere blind.
        const int gezeichnet = gezaehlt(bild);
        // Eine leere Datei faellt hier sofort auf, und zwar fuer jeden Namen.
        QVERIFY2(gezeichnet > 40,
                 qPrintable(QStringLiteral("%1 zeichnet nur %2 Pixel")
                                .arg(name)
                                .arg(gezeichnet)));
    }
#endif
}

void IconsTest::noIconIsAFilledBlock()
{
#ifdef HRT_HAVE_SVG
    for (const QString &name : m_erwartet) {
        // Das Programmsymbol ist als einziges farbig und traegt seinen eigenen
        // Grund; es darf voll sein. Geprueft wird die Strichzeichnung.
        if (name == QLatin1String("app"))
            continue;

        const QImage bild = AppIcons::get(name).pixmap(48, 48).toImage();
        const double anteil = double(gezaehlt(bild)) / double(bild.width() * bild.height());
        // Ein Ring aus 1,7 Strichstaerke fuellt rund ein Fuenftel der Flaeche,
        // eine gefuellte Taste mehr als die Haelfte. Beides heisst: das Bild
        // ist zu schwer oder zu leicht, nicht "ein Zeichen".
        QVERIFY2(anteil > 0.04 && anteil < 0.40,
                 qPrintable(QStringLiteral("%1 deckt %2 % der Flaeche ab")
                                .arg(name)
                                .arg(anteil * 100, 0, 'f', 1)));
    }
#endif
}

void IconsTest::everyMenuEntryCarriesAnIcon()
{
    MainWindow fenster;

    QStringList ohneBild;
    for (QMenu *menu : fenster.menuBar()->findChildren<QMenu *>(
             QString(), Qt::FindDirectChildrenOnly)) {
        for (QAction *action : menu->actions()) {
            if (action->isSeparator())
                continue;
            if (action->icon().isNull())
                ohneBild.append(action->text());
        }
    }
    QVERIFY2(ohneBild.isEmpty(),
             qPrintable(QStringLiteral("ohne Bild im Menü: %1")
                            .arg(ohneBild.join(QStringLiteral(" | ")))));

    QVERIFY(!fenster.windowIcon().isNull());
}

void IconsTest::theActionButtonsCarryIcons()
{
    // Lernen: Runde, Sicher, Unsicher, Geraten, nächste Frage.
    LearningWidget lernen;
    QStringList lernKnoepfe;
    for (QPushButton *knopf : lernen.findChildren<QPushButton *>()) {
        if (!knopf->text().isEmpty())
            lernKnoepfe.append(knopf->text());
        QVERIFY2(!knopf->icon().isNull(),
                 qPrintable(QStringLiteral("Lernen ohne Bild: %1").arg(knopf->text())));
    }
    QVERIFY(!lernKnoepfe.isEmpty());

    // Prüfung: starten, üben, zurück, weiter, auswerten.
    ExamWidget pruefung;
    int mitBild = 0;
    for (QPushButton *knopf : pruefung.findChildren<QPushButton *>()) {
        if (knopf->text().isEmpty())
            continue;
        QVERIFY2(!knopf->icon().isNull(),
                 qPrintable(QStringLiteral("Prüfung ohne Bild: %1").arg(knopf->text())));
        ++mitBild;
    }
    QVERIFY(mitBild >= 5);

    // Prüfbericht: der Kopierknopf.
    ValidationReport bericht;
    bool kopierGefunden = false;
    for (QToolButton *knopf : bericht.findChildren<QToolButton *>()) {
        if (knopf->text().isEmpty())
            continue;
        kopierGefunden = true;
        QVERIFY2(!knopf->icon().isNull(), qPrintable(knopf->text()));
    }
    QVERIFY(kopierGefunden);
}

void IconsTest::noIconGoesUnused()
{
    const QStringList vorhanden = AppIcons::names();
    QStringList ungenutzt;
    for (const QString &name : vorhanden) {
        if (!m_erwartet.contains(name))
            ungenutzt.append(name);
    }
    QVERIFY2(ungenutzt.isEmpty(),
             qPrintable(QStringLiteral("im Satz, aber nirgends benutzt: %1")
                            .arg(ungenutzt.join(QStringLiteral(", ")))));

    QStringList fehlend;
    for (const QString &name : m_erwartet) {
        if (!vorhanden.contains(name))
            fehlend.append(name);
    }
    QVERIFY2(fehlend.isEmpty(),
             qPrintable(QStringLiteral("benutzt, aber nicht im Satz: %1")
                            .arg(fehlend.join(QStringLiteral(", ")))));
}

void IconsTest::theDesktopFileFindsTheIcon()
{
    const QString desktop = QString::fromUtf8(HRT_SOURCE_DIR)
                            + QStringLiteral("/packaging/hamradiotrainer-2026.desktop");
    QFile datei(desktop);
    QVERIFY2(datei.open(QIODevice::ReadOnly), qPrintable(desktop));

    QString gesucht;
    QTextStream zeilen(&datei);
    while (!zeilen.atEnd()) {
        const QString zeile = zeilen.readLine().trimmed();
        if (zeile.startsWith(QLatin1String("Icon=")))
            gesucht = zeile.mid(5).trimmed();
    }
    QVERIFY2(!gesucht.isEmpty(), "die Desktop-Datei nennt kein Symbol");
    QVERIFY2(!gesucht.contains(QLatin1Char('/')),
             "der Name im Themenverzeichnis traegt keinen Pfad");

    // Genau dieser Name muss auch installiert werden.
    QFile cmakelists(QString::fromUtf8(HRT_SOURCE_DIR) + QStringLiteral("/CMakeLists.txt"));
    QVERIFY(cmakelists.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(cmakelists.readAll());
    QVERIFY2(text.contains(QStringLiteral("RENAME %1.svg").arg(gesucht)),
             qPrintable(QStringLiteral("CMakeLists.txt installiert das Symbol nicht als %1.svg")
                            .arg(gesucht)));
}

QTEST_MAIN(IconsTest)
#include "tst_icons.moc"
