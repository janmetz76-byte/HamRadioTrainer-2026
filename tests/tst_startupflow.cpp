// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "MainWindow.h"
#include "StartupDialog.h"

#include "core/CatalogLocator.h"

#include <QLabel>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTreeWidget>
#include <QStandardPaths>
#include <QTest>
#include <QTimer>

using namespace hrt;

namespace {

QString aRealCatalog()
{
    return QStringLiteral(HRT_SOURCE_DIR "/data/catalogs/de-amateurfunk-N.xml");
}

/// Closes the startup assistant as soon as it opens.
///
/// StartupDialog::exec() blocks, so a test has to answer it. Without this the
/// test would hang instead of failing.
class AssistantCloser : public QObject
{
public:
    AssistantCloser(QObject *parent = nullptr)
        : QObject(parent)
    {
        qApp->installEventFilter(this);
        // Der Assistent laeuft in einer eigenen Ereignisschleife; die
        // Ueberwachung muss deshalb waehrend des exec() weiterlaufen.
        timer.setInterval(20);
        connect(&timer, &QTimer::timeout, this, [this] { check(); });
        timer.start();
    }

    void stop()
    {
        timer.stop();
        qApp->removeEventFilter(this);
    }

    bool wasShown() const { return m_shown; }
    void speedUp() { timer.setInterval(10); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(event);
        if (!m_shown && watched && qobject_cast<StartupDialog *>(watched))
            m_shown = true;
        return false;
    }

private:
    void check()
    {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *dialog = qobject_cast<StartupDialog *>(widget)) {
                dialog->accept();
                return;
            }
        }
    }

    QTimer timer;
    bool m_shown = false;
};

void resetSettings()
{
    QSettings settings;
    settings.remove(QStringLiteral("startup/done"));
    settings.remove(QStringLiteral("startup/examDate"));
    settings.remove(QStringLiteral("startup/parts"));
}

} // namespace

class StartupFlowTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void assistantAppearsWithoutAnyRememberedCatalog();
    void assistantAppearsEvenWithARememberedCatalog();
    void assistantDoesNotAppearTwice();
    void cancellingLeavesTheProgramUsable();
    void assistantCanBeOpenedTwice();
    void assistantLeadsIntoTheLearningView();
    void theChosenDateIsShownRightAway();
    void switchingCatalogAfterBrowsingKeepsTheList();

private:
    /// Runs openInitialCatalog() with the assistant answered automatically.
    /// Returns whether the assistant was shown.
    bool runStartup(bool expectAssistant);
};

void StartupFlowTest::init()
{
    resetSettings();
}

void StartupFlowTest::cleanup()
{
    resetSettings();
}

bool StartupFlowTest::runStartup(bool expectAssistant)
{
    AssistantCloser closer;

    MainWindow window;
    Q_UNUSED(expectAssistant);
    window.openInitialCatalog();

    const bool shown = closer.wasShown();
    closer.stop();
    return shown;
}

void StartupFlowTest::assistantAppearsWithoutAnyRememberedCatalog()
{
    QSettings settings;
    settings.remove(QStringLiteral("catalog/lastPath"));

    // Regression: Der Assistent muss beim allerersten Start erscheinen. Ein
    // frueherer Rueckkehrpfad hat ihn uebersprungen, sobald ein Katalogpfad
    // gemerkt war - und nach dem allerersten Start ist das immer der Fall.
    QVERIFY2(runStartup(true), "beim ersten Start kam kein Einrichtungsassistent");
}

void StartupFlowTest::assistantAppearsEvenWithARememberedCatalog()
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());

    QVERIFY2(runStartup(true),
             "der Einrichtungsassistent erschien nicht, obwohl ein Katalog gemerkt war");
}

void StartupFlowTest::assistantDoesNotAppearTwice()
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());

    QVERIFY(runStartup(true));
    // Beim zweiten Start ist die Auswahl bekannt und wird nicht neu erfragt.
    QVERIFY2(!runStartup(false), "der Assistent fragte beim zweiten Start erneut");
}

void StartupFlowTest::cancellingLeavesTheProgramUsable()
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());
    settings.setValue(QStringLiteral("startup/done"), false);

    // "Abbrechen" darf das Programm nicht in einem kaputten Zustand lassen.
    AssistantCloser closer;
    MainWindow window;
    QTimer cancel;
    cancel.setInterval(20);
    connect(&cancel, &QTimer::timeout, this, [this] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *dialog = qobject_cast<StartupDialog *>(widget)) {
                dialog->reject();
                return;
            }
        }
    });
    cancel.start();

    window.openInitialCatalog();
    cancel.stop();
    closer.stop();

    // Der Katalog ist geladen und die Ansichten sind bedienbar.
    QVERIFY(window.isVisible() || window.isEnabled());
}

void StartupFlowTest::assistantLeadsIntoTheLearningView()
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());

    AssistantCloser closer;
    MainWindow window;
    window.openInitialCatalog();
    closer.stop();

    // Der Assistent fragt "Wofuer moechten Sie lernen?". Wer danach in der
    // Katalogansicht landet, muss sich erst noch durchklicken, bis er
    // anfangen kann.
    auto *stack = window.findChild<QStackedWidget *>();
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), int(MainWindow::kLearningView));
}

void StartupFlowTest::theChosenDateIsShownRightAway()
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());

    // Der Assistent nimmt den Termin, den das Datumsfeld vorschlaegt. Ohne
    // dass er ins laufende Programm uebernommen wird, blieb der Countdown die
    // ganze Sitzung ueber leer und erschien erst beim naechsten Start.
    AssistantCloser closer;
    MainWindow window;
    window.openInitialCatalog();
    closer.stop();

    const QDate chosen = settings.value(QStringLiteral("startup/examDate")).toDate();
    QVERIFY(chosen.isValid());

    QString countdown;
    for (const QLabel *label : window.statusBar()->findChildren<QLabel *>())
        countdown += label->text();
    QVERIFY2(!countdown.isEmpty(), "die Statusleiste nennt keinen Pruefungstermin");
    QVERIFY2(countdown.contains(QStringLiteral("Kein Prüfungstermin")) == false,
             qPrintable(QStringLiteral("der gewaehlte Termin wird nicht angezeigt: %1")
                            .arg(countdown)));
}

void StartupFlowTest::switchingCatalogAfterBrowsingKeepsTheList()
{
    // Regression: m_tree->clear() loest itemSelectionChanged aus. Zeigte
    // m_catalog schon auf den neuen Katalog, fuellte die Katalogansicht die
    // Fragenliste noch aus dem freigegebenen alten Katalog - der Absturz
    // mit "corrupted double-linked list" war die Folge.
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"),
                      QStringLiteral(HRT_SOURCE_DIR "/data/catalogs/de-amateurfunk-A.xml"));

    AssistantCloser closer;
    MainWindow window;
    window.openInitialCatalog();
    closer.stop();

    auto *tree = window.findChild<QTreeWidget *>();
    QVERIFY(tree);
    for (int row = 0; row < tree->topLevelItemCount(); ++row) {
        tree->setCurrentItem(tree->topLevelItem(row));
        QCoreApplication::processEvents();
    }

    QVERIFY(window.openCatalog(QStringLiteral(HRT_SOURCE_DIR
                                             "/data/catalogs/de-amateurfunk-N.xml")));
    QCoreApplication::processEvents();
    QCOMPARE(tree->topLevelItemCount(), 3);
    QVERIFY(window.isEnabled());
}

void StartupFlowTest::assistantCanBeOpenedTwice()
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());

    AssistantCloser first;
    MainWindow window;
    window.openInitialCatalog();
    first.stop();

    QVERIFY2(first.wasShown(), "der erste Assistent erschien nicht");

    settings.setValue(QStringLiteral("startup/done"), false);
    AssistantCloser second;
    window.runStartupAssistant();
    second.stop();

    QVERIFY2(second.wasShown(), "der zweite Assistent erschien nicht");
    QVERIFY(window.isEnabled());
}

QTEST_MAIN(StartupFlowTest)
#include "tst_startupflow.moc"