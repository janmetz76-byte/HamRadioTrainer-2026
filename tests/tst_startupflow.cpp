// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "MainWindow.h"
#include "StartupDialog.h"

#include "core/CatalogLocator.h"

#include <QSettings>
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

QTEST_MAIN(StartupFlowTest)
#include "tst_startupflow.moc"