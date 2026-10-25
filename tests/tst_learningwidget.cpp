// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "AnswerView.h"
#include "LearningWidget.h"

#include "core/CatalogXml.h"
#include "learning/ProgressStore.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QtTest>

using namespace hrt;

namespace {

Catalog loadClassCatalog(const char *file)
{
    Catalog catalog;
    QString error;
    const QString path = QDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"))
                             .filePath(QString::fromLatin1(file));
    if (!CatalogXml::load(path, &catalog, &error))
        qFatal("Katalog %s nicht lesbar: %s", file, qPrintable(error));
    return catalog;
}

QString catalogPathFor(const char *file)
{
    return QDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"))
        .filePath(QString::fromLatin1(file));
}

QLabel *findLabelStartingWith(const QWidget *root, const QString &prefix)
{
    for (QLabel *label : root->findChildren<QLabel *>()) {
        if (label->text().startsWith(prefix))
            return label;
    }
    return nullptr;
}

QPushButton *findButton(const QWidget *root, const QString &text)
{
    for (QPushButton *button : root->findChildren<QPushButton *>()) {
        if (button->text() == text)
            return button;
    }
    return nullptr;
}

/// The selectable answers of the question that is currently shown. They live
/// inside the AnswerView; the buttons of the surrounding dialog are not answers
/// and must not be clicked by accident.
QList<QAbstractButton *> answerControls(QWidget *root)
{
    AnswerView *answers = root->findChild<AnswerView *>();
    if (!answers)
        return {};
    QList<QAbstractButton *> controls;
    for (QAbstractButton *button : answers->findChildren<QAbstractButton *>()) {
        if (qobject_cast<QRadioButton *>(button) || qobject_cast<QCheckBox *>(button))
            controls.append(button);
    }
    return controls;
}

} // namespace

class LearningWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void withoutCatalogNothingIsShown();
    void statisticsOfAFreshCatalogAreVisible();
    void statisticsFollowTheStoredProgress();
    void aRoundWalksThroughThePlannedQuestions();
    void theSelfReportIsWhatMovesTheQuestion();
    void aRoundOnlyOverDueQuestionsStaysSmall();
    void thePartFilterNamesTheSheetWithoutTheRegulation();
    void theProgressReachesTheDisk();
    void theSolutionWaitsForTheSelfReport();
    void aRoundGivesTheQuestionTheWholeHeight();
    void theSolutionCanBeSwitchedOff();
    void aNarrowLayoutBeforeTheCatalogKeepsTheStartButton();
};

void LearningWidgetTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void LearningWidgetTest::init()
{
    QSettings().remove(QStringLiteral("learnview/showSolution"));
    QFile::remove(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                      .filePath(QStringLiteral("fortschritt-de-n.json")));
}

void LearningWidgetTest::withoutCatalogNothingIsShown()
{
    LearningWidget widget;
    QVERIFY(findLabelStartingWith(&widget, QStringLiteral("Kein Katalog")) != nullptr);
}

void LearningWidgetTest::statisticsOfAFreshCatalogAreVisible()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));

    QLabel *summary = findLabelStartingWith(&widget, QStringLiteral("0 von"));
    QVERIFY2(summary, "keine Zusammenfassung gefunden");
    QVERIFY2(summary->text().contains(QStringLiteral("571 Fragen")), qPrintable(summary->text()));
    QVERIFY2(summary->text().contains(QStringLiteral("571 neu")), qPrintable(summary->text()));

    // One row per Prüfungsteil.
    QTableWidget *parts = widget.findChild<QTableWidget *>(QStringLiteral("fortschrittJePruefungsteil"));
    QVERIFY2(parts, "keine Tabelle je Prüfungsteil gefunden");
    QCOMPARE(parts->rowCount(), 3);
    QVERIFY(parts->item(0, 0)->text().startsWith(QStringLiteral("V — V (Kenntnisse")));
}

void LearningWidgetTest::statisticsFollowTheStoredProgress()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));

    Scheduler seeded;
    AnswerOutcome outcome;
    outcome.questionId = catalog.parts.first().sections.first().chapters.first()
                              .questions.first().id;
    outcome.correct = true;
    outcome.confidence = Confidence::Certain;
    seeded.record(outcome);
    widget.setProgress(seeded.states());

    QLabel *summary = findLabelStartingWith(&widget, QStringLiteral("1 von"));
    QVERIFY2(summary, "Zusammenfassung aktualisiert sich nicht");
    QVERIFY(summary->text().contains(QStringLiteral("571 Fragen")));
    QVERIFY(!summary->text().contains(QStringLiteral("571 neu")));
}

void LearningWidgetTest::aRoundWalksThroughThePlannedQuestions()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));

    QPushButton *start = findButton(&widget, QStringLiteral("Runde starten"));
    QVERIFY(start);
    start->click();

    QVERIFY(findLabelStartingWith(&widget, QStringLiteral("Frage 1 von")) != nullptr);
    QVERIFY(answerControls(&widget).size() >= 2);

    // Before an answer the self report stays disabled.
    QPushButton *confident = findButton(&widget, QStringLiteral("Sicher"));
    QVERIFY(confident);
    QVERIFY(!confident->isEnabled());

    answerControls(&widget).first()->click();
    QVERIFY2(confident->isEnabled(),
             "die Selbsteinschätzung wird erst nach der Antwort freigegeben");
}

void LearningWidgetTest::theSelfReportIsWhatMovesTheQuestion()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));

    int changes = 0;
    connect(&widget, &LearningWidget::progressChanged, this, [&changes] { ++changes; });

    // A short round, so the test stays quick.
    for (QSpinBox *spin : widget.findChildren<QSpinBox *>()) {
        spin->setValue(3);
        break;
    }
    findButton(&widget, QStringLiteral("Runde starten"))->click();

    for (int step = 0; step < 3; ++step) {
        const QList<QAbstractButton *> answers = answerControls(&widget);
        if (answers.isEmpty())
            break;
        answers.first()->click();

        // The correct answer is revealed, so the control at the same row can be
        // compared against what the widget says.
        QPushButton *confident = findButton(&widget, QStringLiteral("Sicher"));
        QVERIFY(confident);
        QVERIFY(confident->isEnabled());
        confident->click();

        // Then move on, or the round ends by itself.
        if (QPushButton *next = findButton(&widget, QStringLiteral("Nächste Frage"))) {
            if (next->isEnabled())
                next->click();
        }
    }

    QVERIFY2(changes > 0, "die Selbsteinschätzung hat nichts gespeichert");
    QCOMPARE(widget.scheduler().states().size(), 3);

    // The self report is what was stored, whatever the answer was: a wrong
    // answer keeps the question in box 0 but still remembers the certainty.
    QCOMPARE(widget.scheduler().states().size(), 3);
    for (const QuestionState &state : widget.scheduler().states()) {
        QCOMPARE(state.confidence, Confidence::Certain);
        QCOMPARE(state.timesAnswered, 1);
    }
}

void LearningWidgetTest::aRoundOnlyOverDueQuestionsStaysSmall()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));

    Scheduler seeded;
    const QList<Question *> questions = catalog.parts.first().sections.first().questions();
    QVERIFY(questions.size() >= 8);

    // Four questions answered correctly land in box 3 and are not due for a
    // week; four answered wrongly stay in box 0 and are due right away.
    for (int i = 0; i < 4; ++i) {
        AnswerOutcome outcome;
        outcome.questionId = questions.at(i)->id;
        outcome.correct = true;
        outcome.confidence = Confidence::Certain;
        seeded.record(outcome);
    }
    for (int i = 4; i < 8; ++i) {
        AnswerOutcome outcome;
        outcome.questionId = questions.at(i)->id;
        outcome.correct = false;
        outcome.confidence = Confidence::Unsure;
        seeded.record(outcome);
    }
    widget.setProgress(seeded.states());

    for (QCheckBox *box : widget.findChildren<QCheckBox *>()) {
        if (box->text() == QStringLiteral("nur fällige")) {
            box->setChecked(true);
            break;
        }
    }
    // Only the four wrong answers count as repetitions that are due.
    QLabel *summary = findLabelStartingWith(&widget, QStringLiteral("8 von"));
    QVERIFY2(summary, "Zusammenfassung mit den acht geübten Fragen fehlt");
    QVERIFY2(summary->text().contains(QStringLiteral("4 fällig")), qPrintable(summary->text()));

    findButton(&widget, QStringLiteral("Runde starten"))->click();

    // The four securely answered questions are not due and must stay out; the
    // four wrong ones and everything never seen belong to the round.
    // The round length is capped by the spin box, so only the membership is
    // checked here.
    const QStringList plan = widget.currentPlan();
    QVERIFY(!plan.isEmpty());
    for (int i = 0; i < 4; ++i)
        QVERIFY2(!plan.contains(questions.at(i)->id), qPrintable(questions.at(i)->id));
    for (int i = 4; i < 8; ++i)
        QVERIFY2(plan.contains(questions.at(i)->id), qPrintable(questions.at(i)->id));
}

void LearningWidgetTest::thePartFilterNamesTheSheetWithoutTheRegulation()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-A.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-A.xml"));

    // Die Auswahl der Pruefungsteile nennt das Blatt und seinen Inhalt. Die
    // Amtsblatt-Verfuegung steht in der Hilfe - an jeder Zeile der Liste
    // wiederholt, macht sie die Auswahl nur schwer lesbar.
    QComboBox *filter = widget.findChild<QComboBox *>();
    QVERIFY2(filter, "die Auswahl der Pruefungsteile wurde nicht gefunden");
    QCOMPARE(filter->count(), catalog.parts.size() + 1);
    for (int i = 0; i < filter->count(); ++i) {
        const QString entry = filter->itemText(i);
        QVERIFY2(!entry.contains(QStringLiteral("Amtsblatt")), qPrintable(entry));
        QVERIFY2(!entry.contains(QStringLiteral("29/2024")), qPrintable(entry));
    }
    // Und sie sagt trotzdem, welches Blatt gemeint ist.
    QVERIFY(filter->itemText(1).contains(QStringLiteral("Vorschriften")));
}

void LearningWidgetTest::theProgressReachesTheDisk()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    const QString path = catalogPathFor("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, path);

    for (QSpinBox *spin : widget.findChildren<QSpinBox *>()) {
        spin->setValue(2);
        break;
    }
    findButton(&widget, QStringLiteral("Runde starten"))->click();

    const QString file = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                             .filePath(QStringLiteral("fortschritt-de-n.json"));
    ProgressStore store(file);
    // The main window does this in the application; here it is wired by hand.
    connect(&widget, &LearningWidget::progressChanged, this, [&](const Scheduler &scheduler) {
        QString error;
        QVERIFY2(store.save(catalog, scheduler.states(), &error), qPrintable(error));
    });

    for (int step = 0; step < 2; ++step) {
        const QList<QAbstractButton *> answers = answerControls(&widget);
        if (answers.isEmpty())
            break;
        answers.last()->click();
        findButton(&widget, QStringLiteral("Geraten"))->click();
        findButton(&widget, QStringLiteral("Nächste Frage"))->click();

        const ProgressLoad loaded = store.load(catalog);
        QCOMPARE(loaded.states.size(), step + 1);
    }

    QFile::remove(file);
}

void LearningWidgetTest::theSolutionWaitsForTheSelfReport()
{
    // Wie im Trainer von 2015: erst die Antwort waehlen, dann einschaetzen,
    // wie sicher man war - und erst danach erfahren, ob sie stimmte. Wer die
    // Loesung schon sieht, schaetzt nicht mehr sich selbst ein.
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));
    findButton(&widget, QStringLiteral("Runde starten"))->click();

    const QList<QAbstractButton *> answers = answerControls(&widget);
    QVERIFY(answers.size() >= 2);
    answers.first()->click();

    // Noch keine Aufloesung: die Antworten sind weiter waehlbar, kein Haken,
    // kein Text, und weiter geht es erst nach der Einschaetzung.
    for (QAbstractButton *answer : answers)
        QVERIFY(answer->isEnabled());
    for (QLabel *label : widget.findChild<AnswerView *>()->findChildren<QLabel *>())
        QVERIFY2(!(label->isVisible() && label->text() == QStringLiteral("✓")), "Loesung verraten");
    QVERIFY(findLabelStartingWith(&widget, QStringLiteral("Antwort ")) == nullptr);
    QVERIFY(!findButton(&widget, QStringLiteral("Nächste Frage"))->isEnabled());

    // Die Wahl laesst sich vor der Einschaetzung noch aendern.
    answers.at(1)->click();
    QVERIFY(answers.at(1)->isChecked());

    findButton(&widget, QStringLiteral("Unsicher"))->click();

    // Jetzt die Aufloesung, mit dem angezeigten Buchstaben.
    QLabel *feedback = findLabelStartingWith(&widget, QStringLiteral("Antwort B ist "));
    QVERIFY2(feedback, "keine Rueckmeldung nach der Einschaetzung");
    for (QAbstractButton *answer : answers)
        QVERIFY(!answer->isEnabled());
    QVERIFY(!findButton(&widget, QStringLiteral("Unsicher"))->isEnabled());
    QVERIFY(findButton(&widget, QStringLiteral("Nächste Frage"))->isEnabled());
    QCOMPARE(widget.scheduler().states().size(), 1);
    QCOMPARE(widget.scheduler().states().first().confidence, Confidence::Unsure);

    findButton(&widget, QStringLiteral("Nächste Frage"))->click();
    QVERIFY(findLabelStartingWith(&widget, QStringLiteral("Frage 2 von")) != nullptr);
}

void LearningWidgetTest::aRoundGivesTheQuestionTheWholeHeight()
{
    // Bei normaler Fenstergroesse blieb der Frage neben Lernstufen und
    // Teiltabelle nur ein Streifen. Waehrend der Runde tritt der Ueberblick
    // deshalb zurueck und kommt danach mit den neuen Zahlen wieder.
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.resize(1000, 650);
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));
    widget.show();
    QWidget *overview = widget.findChild<QWidget *>(QStringLiteral("lernueberblick"));
    QVERIFY(overview);
    QVERIFY(overview->isVisible());

    for (QSpinBox *spin : widget.findChildren<QSpinBox *>()) {
        spin->setValue(1);
        break;
    }
    findButton(&widget, QStringLiteral("Runde starten"))->click();
    QVERIFY(!overview->isVisible());

    // Die Frage bekommt dann mehr als die Haelfte der Hoehe.
    QCoreApplication::processEvents();
    auto *answers = widget.findChild<AnswerView *>();
    QVERIFY(answers->parentWidget());
    QWidget *scroll = answers->parentWidget()->parentWidget();
    QVERIFY2(scroll->height() > widget.height() / 2,
             qPrintable(QStringLiteral("%1 von %2").arg(scroll->height()).arg(widget.height())));

    answerControls(&widget).first()->click();
    findButton(&widget, QStringLiteral("Sicher"))->click();
    findButton(&widget, QStringLiteral("Nächste Frage"))->click();
    QVERIFY(overview->isVisible());
}

void LearningWidgetTest::theSolutionCanBeSwitchedOff()
{
    // Im Trainer von 2015 liess sich die Rueckmeldung abschalten. Dann gibt
    // die Selbsteinschaetzung die Antwort ab und fuehrt gleich zur naechsten
    // Frage; wie viele stimmten, steht erst am Ende der Runde.
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    LearningWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));

    QCheckBox *show = nullptr;
    for (QCheckBox *box : widget.findChildren<QCheckBox *>()) {
        if (box->text() == QStringLiteral("Lösung nach jeder Antwort zeigen"))
            show = box;
    }
    QVERIFY(show);
    QVERIFY2(show->isChecked(), "die Loesung wird standardmaessig gezeigt");
    show->setChecked(false);

    for (QSpinBox *spin : widget.findChildren<QSpinBox *>()) {
        spin->setValue(2);
        break;
    }
    findButton(&widget, QStringLiteral("Runde starten"))->click();

    answerControls(&widget).first()->click();
    findButton(&widget, QStringLiteral("Sicher"))->click();
    // Keine Aufloesung, sondern gleich die zweite Frage.
    QVERIFY(findLabelStartingWith(&widget, QStringLiteral("Antwort ")) == nullptr);
    QVERIFY(findLabelStartingWith(&widget, QStringLiteral("Frage 2 von 2")) != nullptr);
    for (QAbstractButton *answer : answerControls(&widget))
        QVERIFY(answer->isEnabled());

    answerControls(&widget).first()->click();
    findButton(&widget, QStringLiteral("Sicher"))->click();
    QVERIFY(findLabelStartingWith(&widget, QStringLiteral("Runde beendet:")) != nullptr);
    QCOMPARE(widget.scheduler().states().size(), 2);

    // Die Wahl bleibt fuer den naechsten Start gemerkt.
    QVERIFY(!QSettings().value(QStringLiteral("learnview/showSolution"), true).toBool());
    LearningWidget again;
    for (QCheckBox *box : again.findChildren<QCheckBox *>()) {
        if (box->text() == QStringLiteral("Lösung nach jeder Antwort zeigen"))
            QVERIFY(!box->isChecked());
    }
    QSettings().remove(QStringLiteral("learnview/showSolution"));
}

void LearningWidgetTest::aNarrowLayoutBeforeTheCatalogKeepsTheStartButton()
{
    // Unter Android wird die schmale Anordnung gewaehlt, bevor ein Katalog
    // geladen ist. Das darf "Runde starten" nicht dauerhaft sperren.
    LearningWidget widget;
    widget.setCompact(true);
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));
    QPushButton *start = findButton(&widget, QStringLiteral("Runde starten"));
    QVERIFY(start);
    QVERIFY(start->isEnabled());
    start->click();
    QVERIFY(findLabelStartingWith(&widget, QStringLiteral("Frage 1 von")) != nullptr);
}

QTEST_MAIN(LearningWidgetTest)
#include "tst_learningwidget.moc"