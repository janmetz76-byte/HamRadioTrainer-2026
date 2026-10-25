// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "AnswerView.h"
#include "LearningWidget.h"
#include "MainWindow.h"
#include "StartupDialog.h"
#include "core/CatalogXml.h"
#include "learning/ProgressStore.h"

#include "core/AppIdentity.h"
#include "core/CatalogLocator.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTreeWidget>
#include <QStandardPaths>
#include <QTest>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QToolButton>
#include <QTemporaryDir>
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
    /// Wie auf den Dialog nach einem Katalogwechsel geantwortet wird.
    enum ProgressChoice { Keep, Restart };

    AssistantCloser(ProgressChoice choice = Keep, QObject *parent = nullptr)
        : QObject(parent)
        , m_choice(choice)
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
    /// Schliesst den Assistenten und jeden anderen modalen Dialog, der sich
    /// dazwischenschiebt.
    void check()
    {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *dialog = qobject_cast<StartupDialog *>(widget)) {
                dialog->accept();
                return;
            }
            // Der Dialog nach einem Katalogwechsel gehoert zum Verhalten, nicht
            // zum Fehler. Standardmaessig wird die erste Schaltflaeche
            // gedrueckt - das ist "Fortschritt uebernehmen", also die
            // Entscheidung, die keine Arbeit kostet.
            if (auto *meldung = qobject_cast<QMessageBox *>(widget)) {
                const QList<QAbstractButton *> knoepfe = meldung->buttons();
                if (knoepfe.isEmpty()) {
                    meldung->accept();
                    return;
                }
                // Der Dialog bietet "Fortschritt übernehmen" zuerst an.
                const int index = m_choice == Keep ? 0 : 1;
                knoepfe.at(qMin(index, knoepfe.size() - 1))->click();
                return;
            }
        }
    }

    QTimer timer;
    bool m_shown = false;
    ProgressChoice m_choice = Keep;
};

/// Liest den Text eines Dialogs, bevor er geschlossen wird.
///
/// Der Dialog ueber die Herkunft der Fragen ist genau der Text, den der
/// Lernende sieht. Ihn zu testen, statt eine private Hilfsmethode
/// oeffentlich zu machen, haelt die Oberflaeche unangetastet.
class DialogReader : public QObject
{
public:
    explicit DialogReader(QObject *parent = nullptr)
        : QObject(parent)
    {
        qApp->installEventFilter(this);
        timer.setInterval(20);
        connect(&timer, &QTimer::timeout, this, [this] { check(); });
        timer.start();
    }

    void stop()
    {
        timer.stop();
        qApp->removeEventFilter(this);
    }

    QString text() const { return m_text; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(event);
        if (!m_seen && watched && qobject_cast<QMessageBox *>(watched))
            m_seen = true;
        return false;
    }

private:
    void check()
    {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *meldung = qobject_cast<QMessageBox *>(widget);
            if (!meldung)
                continue;
            m_text = meldung->informativeText();
            const QList<QAbstractButton *> knoepfe = meldung->buttons();
            if (!knoepfe.isEmpty())
                knoepfe.first()->click();
            else
                meldung->accept();
            return;
        }
    }

    QTimer timer;
    bool m_seen = false;
    QString m_text;
};

void resetSettings()
{
    QSettings settings;
    settings.remove(QStringLiteral("startup/done"));
    settings.remove(QStringLiteral("startup/examDate"));
    settings.remove(QStringLiteral("startup/parts"));
    settings.remove(QStringLiteral("startup/combination"));
}

} // namespace

class StartupFlowTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
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
    void theMenuBarHasThreeEntries();
    void theUpdateEntrySitsWithTheCatalogs();
    void aChangedCatalogKeepsTheLearningProgress();
    void choosingToStartOverDropsTheLearningProgress();
    void whatWasLearnedSurvivesARestart();
    void anAscentLearnsOnlyItsOwnParts();
    void theHelpNamesTheSourceAndItsLicence();
    void theSidebarCarriesItsNameAndItsGroups();
    void theSidebarCanBeFoldedAway();
    void theLearningViewIsBuiltFromNamedBlocks();

private:
    /// Runs openInitialCatalog() with the assistant answered automatically.
    /// Returns whether the assistant was shown.
    bool runStartup(bool expectAssistant);
};

void StartupFlowTest::initTestCase()
{
    // Dieselben Einstellungsnamen wie das Programm: ohne sie legt QSettings
    // unter Windows nichts Brauchbares ab, und der Test pruefte auf dieser
    // Plattform einen anderen Speicherort als die Anwendung.
    hrt::setApplicationIdentity();
    QStandardPaths::setTestModeEnabled(true);
}

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

    // Nicht "irgendein QTreeWidget": der Pruefbericht enthaelt selbst eines,
    // und welches findChild zuerst liefert, ist nicht zugesichert.
    auto *tree = window.findChild<QTreeWidget *>(QStringLiteral("kapitelbaum"));
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

void StartupFlowTest::theMenuBarHasThreeEntries()
{
    // Datei, Prüfbericht, Hilfe - mehr nicht. Der Wegweiser des Prüfberichts
    // lag früher als freistehender Punkt auf der Menüleiste und war damit
    // weder erklärend noch auffindbar; jetzt hängt er im eigenen Menü.
    MainWindow window;
    const QList<QMenu *> menus = window.menuBar()->findChildren<QMenu *>(
        QString(), Qt::FindDirectChildrenOnly);
    QStringList names;
    for (QMenu *menu : menus)
        names.append(menu->title().remove(QLatin1Char('&')));
    QCOMPARE(names, QStringList({QStringLiteral("Datei"), QStringLiteral("Prüfbericht"),
                                 QStringLiteral("Hilfe")}));

    QMenu *report = nullptr;
    for (QMenu *menu : menus) {
        if (menu->title().remove(QLatin1Char('&')) == QStringLiteral("Prüfbericht"))
            report = menu;
    }
    QVERIFY(report);
    QStringList entries;
    for (QAction *action : report->actions())
        entries.append(action->text());
    QVERIFY2(entries.filter(QStringLiteral("anzeigen")).size() == 1,
             qPrintable(entries.join(QStringLiteral(" | "))));
    QVERIFY2(entries.filter(QStringLiteral("erneut prüfen")).size() == 1,
             qPrintable(entries.join(QStringLiteral(" | "))));

    // Ohne Katalog gibt es nichts zu pruefen: blass, aber sichtbar.
    QVERIFY(!entries.isEmpty());
    for (QAction *action : report->actions()) {
        if (action->text().contains(QStringLiteral("erneut prüfen")))
            QVERIFY(!action->isEnabled());
    }

    // Mit Katalog werden sie scharf.
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());
    MainWindow geladen;
    AssistantCloser closer;
    geladen.openInitialCatalog();
    closer.stop();
    QMenu *report2 = nullptr;
    for (QMenu *menu : geladen.menuBar()->findChildren<QMenu *>(
             QString(), Qt::FindDirectChildrenOnly)) {
        if (menu->title().remove(QLatin1Char('&')) == QStringLiteral("Prüfbericht"))
            report2 = menu;
    }
    QVERIFY(report2);
    bool checked = false;
    for (QAction *action : report2->actions()) {
        if (action->text().contains(QStringLiteral("erneut prüfen"))) {
            QVERIFY(action->isEnabled());
            checked = true;
        }
    }
    QVERIFY(checked);
}

void StartupFlowTest::theUpdateEntrySitsWithTheCatalogs()
{
    // Die Aktualisierung schreibt genau die Dateien, die "Katalog öffnen" liest.
    // Sie gehoert deshalb unter "Datei" und nicht in ein eigenes Menü oder an
    // den Rand der Menüleiste.
    MainWindow window;
    QMenu *datei = nullptr;
    for (QMenu *menu : window.menuBar()->findChildren<QMenu *>(
             QString(), Qt::FindDirectChildrenOnly)) {
        if (menu->title().remove(QLatin1Char('&')) == QStringLiteral("Datei"))
            datei = menu;
    }
    QVERIFY2(datei, "das Menü Datei fehlt");

    QStringList entries;
    for (QAction *action : datei->actions())
        entries.append(action->text().remove(QLatin1Char('&')));
    QCOMPARE(entries.filter(QStringLiteral("Fragenkatalog aktualisieren")).size(), 1);

    // Ohne Katalog gibt es kein Verzeichnis, in das geschrieben werden
    // könnte: blass, aber auffindbar.
    for (QAction *action : datei->actions()) {
        if (action->text().contains(QStringLiteral("aktualisieren")))
            QVERIFY(!action->isEnabled());
    }

    // Mit Katalog wird sie scharf.
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());
    MainWindow geladen;
    AssistantCloser closer;
    geladen.openInitialCatalog();
    closer.stop();

    for (QMenu *menu : geladen.menuBar()->findChildren<QMenu *>(
             QString(), Qt::FindDirectChildrenOnly)) {
        if (menu->title().remove(QLatin1Char('&')) != QStringLiteral("Datei"))
            continue;
        for (QAction *action : menu->actions()) {
            if (action->text().contains(QStringLiteral("aktualisieren")))
                QVERIFY2(action->isEnabled(), "mit geladenem Katalog muss sie scharf sein");
        }
    }
}

void StartupFlowTest::aChangedCatalogKeepsTheLearningProgress()
{
    // Der Punkt des ganzen Katalogwechsels: wer monatelang gelernt hat, darf
    // nicht bei null anfangen, nur weil der Katalog ein paar Zeichen weiter
    // ist. Der Test schreibt einen Fortschritt zu einem fremden Katalogeinstand
    // und prueft danach, dass er die Aktualisierung ueberlebt.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString katalog = dir.filePath(QStringLiteral("de-amateurfunk-N.xml"));
    QVERIFY2(QFile::copy(aRealCatalog(), katalog), "der Katalog liess sich nicht kopieren");

    hrt::Catalog original;
    QString error;
    QVERIFY2(hrt::CatalogXml::load(katalog, &original, &error), qPrintable(error));

    // Zwei beantwortete Fragen, gebunden an eine andere Revision der Datei.
    QVector<hrt::QuestionState> states;
    for (int i = 0; i < 2 && i < original.questions().size(); ++i) {
        hrt::QuestionState state;
        state.questionId = original.questions().at(i)->id;
        state.box = 4;
        state.timesAnswered = 8;
        state.timesCorrect = 4;
        state.confidence = hrt::Confidence::FairlySure;
        states.append(state);
    }
    QVERIFY(!states.isEmpty());

    hrt::Catalog fremd = original;
    fremd.sourceDigest = QStringLiteral("digest-von-gestern");
    hrt::ProgressStore store(hrt::ProgressStore::defaultPath(fremd));
    QVERIFY2(store.save(fremd, states, &error), qPrintable(error));

    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), katalog);
    settings.setValue(QStringLiteral("startup/done"), true);

    MainWindow window;
    AssistantCloser closer;
    window.openInitialCatalog();
    closer.stop();

    // Der Fortschritt gehoert jetzt zum neuen Katalogeinstand und ist noch
    // vollstaendig. Die Datei wird beim Laden neu gebunden, damit derselbe
    // Dialog nicht bei jedem Start erneut erscheint.
    const hrt::ProgressLoad nachher = store.load(original);
    QVERIFY2(nachher.error.isEmpty(), qPrintable(nachher.error));
    QVERIFY2(!nachher.catalogChanged, "der Fortschritt wurde nicht neu gebunden");
    QCOMPARE(nachher.states.size(), states.size());
    QCOMPARE(nachher.states.first().box, 4);
    QCOMPARE(nachher.keptStates, states.size());
}

void StartupFlowTest::choosingToStartOverDropsTheLearningProgress()
{
    // Das Angebot muss in beide Richtungen funktionieren. Wer den Katalog
    // gewechselt hat, weil die neuen Fragen doch nichts für ihn sind, soll den
    // alten Fortschritt loswerden können - dafür gibt es die zweite Taste.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString katalog = dir.filePath(QStringLiteral("de-amateurfunk-N.xml"));
    QVERIFY2(QFile::copy(aRealCatalog(), katalog), "der Katalog liess sich nicht kopieren");

    hrt::Catalog original;
    QString error;
    QVERIFY2(hrt::CatalogXml::load(katalog, &original, &error), qPrintable(error));

    QVector<hrt::QuestionState> states;
    hrt::QuestionState state;
    state.questionId = original.questions().first()->id;
    state.box = 5;
    state.timesAnswered = 10;
    states.append(state);

    hrt::Catalog fremd = original;
    fremd.sourceDigest = QStringLiteral("digest-von-gestern");
    hrt::ProgressStore store(hrt::ProgressStore::defaultPath(fremd));
    QVERIFY2(store.save(fremd, states, &error), qPrintable(error));

    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), katalog);
    settings.setValue(QStringLiteral("startup/done"), true);

    MainWindow window;
    AssistantCloser closer(AssistantCloser::Restart);
    window.openInitialCatalog();
    closer.stop();

    const hrt::ProgressLoad nachher = store.load(original);
    QVERIFY2(nachher.error.isEmpty(), qPrintable(nachher.error));
    QVERIFY2(nachher.states.isEmpty(),
             "Neu beginnen muss den alten Fortschritt tatsaechlich verwerfen");
    QCOMPARE(nachher.keptStates, 0);
}

void StartupFlowTest::whatWasLearnedSurvivesARestart()
{
    // Wer mitten in einer Lernrunde das Programm schliesst, darf beim
    // naechsten Start nicht bei null stehen. Lern- und Pruefungsansicht
    // fuehren je einen eigenen Stand; gespeichert wurde frueher nur der der
    // Pruefung, und alles in der Lernansicht Beantwortete ging verloren.
    hrt::Catalog catalog;
    QString error;
    QVERIFY2(hrt::CatalogXml::load(aRealCatalog(), &catalog, &error), qPrintable(error));
    const QString file = hrt::ProgressStore::defaultPath(catalog);
    QFile::remove(file);

    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());
    settings.setValue(QStringLiteral("startup/done"), true);

    {
        AssistantCloser closer;
        MainWindow window;
        window.openInitialCatalog();
        closer.stop();

        auto *learning = window.findChild<hrt::LearningWidget *>();
        QVERIFY(learning);
        for (QSpinBox *spin : learning->findChildren<QSpinBox *>()) {
            spin->setValue(5);
            break;
        }
        QPushButton *start = nullptr;
        QPushButton *sure = nullptr;
        for (QPushButton *button : learning->findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("Runde starten"))
                start = button;
            if (button->text() == QStringLiteral("Sicher"))
                sure = button;
        }
        QVERIFY(start && sure);
        start->click();

        // Zwei von fuenf Fragen, dann wird das Fenster geschlossen - die
        // Runde ist nicht zu Ende.
        for (int step = 0; step < 2; ++step) {
            auto *answers = learning->findChild<hrt::AnswerView *>();
            QVERIFY(answers);
            QAbstractButton *first = nullptr;
            for (QAbstractButton *button : answers->findChildren<QAbstractButton *>()) {
                if (qobject_cast<QRadioButton *>(button) || qobject_cast<QCheckBox *>(button)) {
                    first = button;
                    break;
                }
            }
            QVERIFY(first);
            first->click();
            sure->click();
            for (QPushButton *button : learning->findChildren<QPushButton *>()) {
                if (button->text() == QStringLiteral("Nächste Frage"))
                    button->click();
            }
        }
    }

    const hrt::ProgressLoad loaded = hrt::ProgressStore(file).load(catalog);
    QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
    QCOMPARE(loaded.states.size(), 2);

    // Und das neu gestartete Programm zeigt ihn auch an.
    AssistantCloser closer;
    MainWindow again;
    again.openInitialCatalog();
    closer.stop();
    auto *learning = again.findChild<hrt::LearningWidget *>();
    QVERIFY(learning);
    QCOMPARE(learning->scheduler().states().size(), 2);
    QFile::remove(file);
}

void StartupFlowTest::anAscentLearnsOnlyItsOwnParts()
{
    // Wer die Klasse N schon hat und nach E aufsteigt, lernt nur noch die
    // Technik Klasse E. Frueher galt die Wahl im Assistenten allein fuer die
    // Pruefungssimulation; die Lernansicht bot weiter den ganzen Katalog an.
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());
    settings.setValue(QStringLiteral("startup/done"), false);

    QTimer answer;
    answer.setInterval(20);
    connect(&answer, &QTimer::timeout, this, [] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *dialog = qobject_cast<StartupDialog *>(widget);
            if (!dialog)
                continue;
            for (QRadioButton *button : dialog->findChildren<QRadioButton *>()) {
                if (button->text() == QStringLiteral("Von Klasse N nach E"))
                    button->setChecked(true);
            }
            dialog->accept();
            return;
        }
    });
    answer.start();
    AssistantCloser closer;
    MainWindow window;
    window.openInitialCatalog();
    answer.stop();
    closer.stop();

    // Geoeffnet ist der Katalog der Klasse E, gemerkt der gewaehlte Fall.
    QVERIFY2(settings.value(QStringLiteral("catalog/lastPath")).toString().endsWith(
                 QStringLiteral("de-amateurfunk-E.xml")),
             qPrintable(settings.value(QStringLiteral("catalog/lastPath")).toString()));
    QCOMPARE(settings.value(QStringLiteral("startup/combination")).toString(),
             QStringLiteral("N-E"));

    // Die Teilauswahl der Lernansicht kennt nur noch "alle" und den Teil E.
    auto *learning = window.findChild<hrt::LearningWidget *>();
    QVERIFY(learning);
    QComboBox *filter = nullptr;
    for (QComboBox *box : learning->findChildren<QComboBox *>()) {
        if (box->findData(QString()) == 0)
            filter = box;
    }
    QVERIFY(filter);
    QCOMPARE(filter->count(), 2);
    QCOMPARE(filter->itemData(1).toString(), QStringLiteral("E"));

    // Und eine Runde ueber "alle" stellt nur Fragen aus dem Teil E.
    for (QSpinBox *spin : learning->findChildren<QSpinBox *>()) {
        spin->setValue(20);
        break;
    }
    for (QPushButton *button : learning->findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("Runde starten"))
            button->click();
    }
    const QStringList plan = learning->currentPlan();
    QVERIFY(!plan.isEmpty());
    for (const QString &id : plan)
        QVERIFY2(id.startsWith(QLatin1Char('E')), qPrintable(id));
}

void StartupFlowTest::theHelpNamesTheSourceAndItsLicence()
{
    // Die Hilfe muss sagen, woher die Fragen kommen und unter welcher Lizenz
    // sie stehen - in der Reihenfolge, in der ein Leser es braucht: erst die
    // Fragenbasis, dann der Herausgeber, dann die Lizenz im Klartext und als
    // Kennung zum Nachschlagen.
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());
    settings.setValue(QStringLiteral("startup/done"), true);

    MainWindow window;
    AssistantCloser closer;
    window.openInitialCatalog();
    closer.stop();

    // "Über HamRadioTrainer 2026" ausloesen, wie ein Lernender es tut.
    QAction *about = nullptr;
    for (QMenu *menu : window.menuBar()->findChildren<QMenu *>(
             QString(), Qt::FindDirectChildrenOnly)) {
        for (QAction *action : menu->actions()) {
            if (action->text().remove(QLatin1Char('&')).startsWith(QStringLiteral("Über")))
                about = action;
        }
    }
    QVERIFY2(about, "der Eintrag Über fehlt im Menü Hilfe");

    DialogReader leser;
    about->trigger();
    leser.stop();

    const QString text = leser.text();
    QVERIFY2(!text.isEmpty(), "der Dialog brachte keinen Text");

    QVERIFY2(text.contains(QStringLiteral("Fragenbasis: Prüfungsfragen zum Erwerb von "
                                          "Amateurfunkprüfungsbescheinigungen")),
             qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Herausgeber: Bundesnetzagentur für "
                                          "Elektrizität, Gas, Telekommunikation, Post "
                                          "und Eisenbahnen, Tulpenfeld 4, 53113 Bonn")),
             qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Lizenz Fragenkatalog: Datenlizenz Deutschland "
                                          "– Namensnennung – Version 2.0 "
                                          "(www.govdata.de/dl-de/by-2-0)")),
             qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Datenlizenz: DL-DE-BY-2.0")), qPrintable(text));

    // Die Lizenz steht nicht mehr mitten im Satz der Fragenbasis.
    QVERIFY2(!text.contains(QStringLiteral("amateurfunk), Datenlizenz")),
             "die Lizenz gehoert auf ihre eigene Zeile, nicht in den Quellenvermerk");

    // Der alte Einleitungssatz ist entfallen: er sprach von einem Umfang, um
    // den es hier nicht geht.
    QVERIFY2(!text.contains(QStringLiteral("Amtsblatt-Verfügung")), qPrintable(text));
}

void StartupFlowTest::theSidebarCarriesItsNameAndItsGroups()
{
    // Die Leiste heisst "Menü" und hat eine Gruppenueberschrift. Ohne beides
    // ist eine Liste mit drei Knoepfen links kein Menü, sondern eine
    // Gewohnheit - und die Ueberschrift erklaert, warum überhaupt alle drei
    // dasselbe Lernen betreffen.
    MainWindow fenster;

    QWidget *leiste = nullptr;
    for (QWidget *w : fenster.findChildren<QWidget *>()) {
        if (w->objectName() == QLatin1String("ansichtsleiste"))
           leiste = w;
    }
    QVERIFY2(leiste, "die Ansichtsleiste hat keinen Namen");

    QStringList texte;
    QStringList gruppen;
    int knoepfe = 0;
    for (QAbstractButton *knopf : leiste->findChildren<QAbstractButton *>()) {
        ++knoepfe;
        texte.append(knopf->text());
        // Jeder Ansichtsumschalter traegt ein Bild - das ist der ganze Sinn
        // der Leiste.
        QVERIFY2(!knopf->icon().isNull(), qPrintable(knopf->text()));
    }
    for (QLabel *label : leiste->findChildren<QLabel *>())
        gruppen.append(label->text());

    // Drei Ansichten und der Einklapp-Knopf, seit der Statistik vier Ansichten
    // und derselbe eine Knopf. Die Zahl wird hier festgeschrieben und nicht
    // mitgezählt: Eine Ansicht, die man hinzufuegt, soll diesen Test erst
    // einmal rot zeigen, damit man sie bedenkt - und nicht still verschwinden.
    QCOMPARE(knoepfe, 5); // vier Ansichten und der Einklapp-Knopf
    QVERIFY(texte.contains(QStringLiteral("Katalogansicht")));
    QVERIFY(texte.contains(QStringLiteral("Fragen lernen")));
    QVERIFY(texte.contains(QStringLiteral("Prüfungssimulation")));
    QVERIFY(texte.contains(QStringLiteral("Statistik")));
    QVERIFY(gruppen.contains(QStringLiteral("Menü")));
    QVERIFY(gruppen.contains(QStringLiteral("Lernen")));

    // Genau eine Ansicht ist gewaehlt, und es ist eine der drei.
    int gewaehlt = 0;
    for (QAbstractButton *knopf : leiste->findChildren<QAbstractButton *>()) {
        auto *umschalter = qobject_cast<QToolButton *>(knopf);
        if (!umschalter || !umschalter->isCheckable())
            continue;
        if (umschalter->isChecked())
            ++gewaehlt;
    }
    QCOMPARE(gewaehlt, 1);
}

void StartupFlowTest::theSidebarCanBeFoldedAway()
{
    // Wer eine lange Tabelle liest, braucht Platz. Die Leiste laesst sich
    // einklappen, und der Knopf zum Zurueck bleibt sichtbar - sonst waere
    // sie weg und nicht wiederzufinden.
    MainWindow fenster;
    QWidget *leiste = nullptr;
    QToolButton *knopf = nullptr;
    for (QWidget *w : fenster.findChildren<QWidget *>()) {
        if (w->objectName() == QLatin1String("ansichtsleiste"))
           leiste = w;
    }
    QVERIFY(leiste);
    for (QToolButton *k : leiste->findChildren<QToolButton *>()) {
        if (k->objectName() == QLatin1String("leisteEinklappen"))
            knopf = k;
    }
    QVERIFY2(knopf, "der Knopf zum Einklappen fehlt");

    const int breit = leiste->width();
    knopf->click();
    QVERIFY2(leiste->width() < breit, "die Leiste wurde nicht schmaler");
    QVERIFY(!knopf->icon().isNull());

    knopf->click();
    QCOMPARE(leiste->width(), breit);
}

void StartupFlowTest::theLearningViewIsBuiltFromNamedBlocks()
{
    // Die Lernansicht besteht aus benannten Bloecken: erst der Lernstand,
    // dann die Uebungsrunde, dann die Frage, zuletzt die Loesungshinweise.
    // Eine lange Seite ohne Zwischenueberschriften laesst sich nicht lesen.
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), aRealCatalog());
    settings.setValue(QStringLiteral("startup/done"), true);

    MainWindow fenster;
    AssistantCloser closer;
    fenster.openInitialCatalog();
    closer.stop();

    QStringList balken;
    for (QLabel *w : fenster.findChildren<QLabel *>()) {
        const QString text = w->text();
        // QStringLiteral und nicht QLatin1String: letzteres liest die
        // UTF-8-Bytes der Umlaute als Latin-1 und vergleicht dann
        // "Ue" gegen "Ãœ" - der Vergleich faellt immer durch und der Test
        // waere blind fuer genau die Beschriftung, die er pruefen soll.
        if (text == QStringLiteral("Lernfortschritt")
            || text == QStringLiteral("Übungsrunde")
            || text == QStringLiteral("Lösungshinweise")) {
            balken.append(text);
        }
    }
    QVERIFY2(balken.contains(QStringLiteral("Lernfortschritt")),
             qPrintable(balken.join(QStringLiteral(" | "))));
    QVERIFY2(balken.contains(QStringLiteral("Übungsrunde")), qPrintable(balken.join(QStringLiteral(" | "))));
}

QTEST_MAIN(StartupFlowTest)
#include "tst_startupflow.moc"