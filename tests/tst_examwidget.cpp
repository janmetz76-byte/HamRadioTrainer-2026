// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ExamWidget.h"
#include "AnswerView.h"

#include "core/CatalogXml.h"
#include "learning/ProgressStore.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimer>
#include <QScopedPointer>
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

/// Label whose text starts with the given prefix.
QLabel *findLabel(const QWidget *root, const QString &prefix)
{
    const QList<QLabel *> labels = root->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label->text().startsWith(prefix))
            return label;
    }
    return nullptr;
}

} // namespace

/// The widget itself carries no verdict, so these tests only prove that
/// starting, presenting and grading a paper runs without a display. The pass
/// marks themselves are checked in ExamSessionTest against the real catalogs.
class ExamWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void withoutCatalogNothingCanBeStarted();
    void examPresentsTheFirstQuestionOfTheFirstPart();
    void trainingPaperHasNoPassMarkInTheHeader();
    void answersCanBeSelectedAndReplaced();
    void imageOnlyAnswersShowTheirImageNextToTheChoice();
    void clickingAnAnswerImageSelectsIt();
    void catalogChangeDiscardsTheRunningPaper();
    void progressSurvivesAFullSession();
};

/// The whole learning loop: answering a paper updates the progress, the
/// progress reaches the disk, and a freshly started application finds it again.
void ExamWidgetTest::progressSurvivesAFullSession()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    const QString path = catalogPathFor("de-amateurfunk-N.xml");
    QScopedPointer<ExamWidget> widget(new ExamWidget);
    widget->setCatalog(&catalog, path);

    QVERIFY(widget->startTraining(8));
    QVERIFY(widget->scheduler().states().isEmpty());

    int answered = 0;
    for (int i = 0; i < 8; ++i) {
        for (int k = 0; k < 5; ++k)
            QCoreApplication::processEvents();
        QScrollArea *area = widget->findChild<QScrollArea *>();
        QVERIFY(area);
        const QList<QAbstractButton *> buttons = area->findChildren<QAbstractButton *>();
        if (buttons.isEmpty())
            break;
        // Always pick the last answer, so some answers are wrong on purpose and
        // the boxes actually move.
        buttons.last()->click();
        ++answered;
        for (QPushButton *b : widget->findChildren<QPushButton *>()) {
            if (b->text().startsWith(QStringLiteral("Nächste"))) {
                b->click();
                break;
            }
        }
    }
    QCOMPARE(answered, 8);

    // The progress is only updated when the paper is graded, because that is
    // the moment the outcome of every question is known.
    ExamResult graded;
    connect(widget.data(), &ExamWidget::examFinished, this,
            [&graded](const ExamResult &result) { graded = result; });
    QVERIFY(QMetaObject::invokeMethod(widget.data(), "finishExam"));
    QCOMPARE(graded.totalQuestions, 8);
    QCOMPARE(graded.wrongAnswers + graded.achievedPoints, 8);
    QCOMPARE(widget->scheduler().states().size(), 8);
    QVERIFY(widget->scheduler().weightFor(widget->scheduler().states().first()) >= 0.0);

    // The application saves on progressChanged.
    const QString file = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                             .filePath(QStringLiteral("fortschritt-de-n.json"));
    QFile::remove(file);
    const ProgressStore store(file);
    QVERIFY(store.save(catalog, widget->scheduler().states(), nullptr));

    // A new widget, as after a restart, must start with this progress.
    QScopedPointer<ExamWidget> restarted(new ExamWidget);
    restarted->setCatalog(&catalog, path);
    const ProgressLoad loaded = store.load(catalog);
    QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
    QCOMPARE(loaded.states.size(), 8);
    restarted->setProgress(loaded.states);
    QCOMPARE(restarted->scheduler().states().size(), 8);

    QFile::remove(file);
}

void ExamWidgetTest::initTestCase()
{
    // Keep the test away from the real application data directory.
    QStandardPaths::setTestModeEnabled(true);
}

void ExamWidgetTest::withoutCatalogNothingCanBeStarted()
{
    ExamWidget widget;
    QVERIFY(!widget.startExam());
    QVERIFY(!widget.startTraining(10));
}

void ExamWidgetTest::examPresentsTheFirstQuestionOfTheFirstPart()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-E.xml");
    ExamWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-E.xml"));

    QVERIFY(widget.startExam());

    // Prüfungsteil V comes first in the catalog and holds 25 of the 100
    // questions, and the pass mark is stated per part.
    QLabel *header = findLabel(&widget, QStringLiteral("Prüfungsteil V"));
    QVERIFY2(header, qPrintable(QStringLiteral("Kopfzeile nicht gefunden")));
    QVERIFY2(header->text().contains(QStringLiteral("Frage 1 von 100")), qPrintable(header->text()));
    QVERIFY2(header->text().contains(QStringLiteral("Bestehen ab 76 von 100 Punkten")),
             qPrintable(header->text()));

    QVERIFY(findLabel(&widget, QStringLiteral("0 von 100 beantwortet")) != nullptr);

    // The answers of the drawn question are offered as selectable buttons.
    const QList<QRadioButton *> radios = widget.findChildren<QRadioButton *>();
    const QList<QCheckBox *> boxes = widget.findChildren<QCheckBox *>();
    QVERIFY2(!radios.isEmpty() || !boxes.isEmpty(), "keine Antwortmöglichkeit erzeugt");
}

void ExamWidgetTest::trainingPaperHasNoPassMarkInTheHeader()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    ExamWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));

    QVERIFY(widget.startTraining(10));

    QLabel *header = findLabel(&widget, QStringLiteral("Training —"));
    QVERIFY2(header, qPrintable(QStringLiteral("Kopfzeile nicht gefunden")));
    QVERIFY2(header->text().contains(QStringLiteral("Frage 1 von 10")), qPrintable(header->text()));
    QVERIFY2(!header->text().contains(QStringLiteral("Bestehen")),
             "ein Trainingspapier darf keine Bestehensschwelle vortäuschen");
}

void ExamWidgetTest::answersCanBeSelectedAndReplaced()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    ExamWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));
    QVERIFY(widget.startTraining(5));

    // The answers live in the scroll area; the start and navigation buttons sit
    // outside it. The answer controls carry no text of their own, because the
    // answer text is shown in a wrapping label next to them.
    QScrollArea *area = widget.findChild<QScrollArea *>();
    QVERIFY2(area, "kein Bereich für die Antworten gefunden");
    const QList<QAbstractButton *> buttons = area->findChildren<QAbstractButton *>();
    QVERIFY2(buttons.size() >= 2, qPrintable(QStringLiteral("nur %1 Antwortknöpfe").arg(buttons.size())));

    QAbstractButton *first = buttons.first();
    first->click();
    QVERIFY(first->isChecked());
    QVERIFY(findLabel(&widget, QStringLiteral("1 von 5 beantwortet")) != nullptr);

    // Selecting another answer replaces the previous one, the paper never ends
    // up with two answers to the same question.
    QAbstractButton *second = buttons.at(1);
    second->click();
    QVERIFY(second->isChecked());
    QVERIFY(!first->isChecked());
    QVERIFY(findLabel(&widget, QStringLiteral("1 von 5 beantwortet")) != nullptr);
}

void ExamWidgetTest::imageOnlyAnswersShowTheirImageNextToTheChoice()
{
    // Frage NI104 aus Klasse N hat vier Antworten, die nur aus einem
    // Schaltbild bestehen. Das leere Textlabel durfte keinen Platz
    // beanspruchen und das Bild von der Auswahl wegschieben.
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    const Question *question = nullptr;
    for (Question *candidate : catalog.questions()) {
        if (candidate && candidate->id == QStringLiteral("NI104")) {
            question = candidate;
            break;
        }
    }
    QVERIFY2(question, "Frage NI104 nicht im Katalog N");
    QVERIFY(!question->answers.isEmpty());

    // Die Bildantwort steht nicht mehr an erster Stelle: der Import verteilt
    // die Position der richtigen Antwort ueber alle vier Plaetze. Gesucht
    // wird deshalb nach dem Bild, nicht nach der Position.
    int imageAnswer = -1;
    for (int i = 0; i < question->answers.size(); ++i) {
        if (question->answers.at(i).image.contains(QStringLiteral("NI104_a.svg"))) {
            imageAnswer = i;
            break;
        }
    }
    QVERIFY2(imageAnswer >= 0, "die Antwort mit NI104_a.svg fehlt im Katalog N");
    QVERIFY(question->answers.at(imageAnswer).text.simplified().isEmpty());

    AnswerView *view = new AnswerView;
    QVector<int> order;
    for (int i = 0; i < question->answers.size(); ++i)
        order.append(i);
    view->setQuestion(question, order, QDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs")));
    view->resize(700, 600);
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));

    QList<QLabel *> imageLabels;
    for (QLabel *label : view->findChildren<QLabel *>()) {
        if (!label->pixmap().isNull())
            imageLabels.append(label);
    }
    QVERIFY2(imageLabels.size() >= question->answers.size(),
             qPrintable(QStringLiteral("nur %1 Bildlabels fuer %2 Antworten")
                            .arg(imageLabels.size())
                            .arg(question->answers.size())));

    // Das Bild muss unmittelbar neben der Auswahl stehen, nicht weit weg auf
    // der anderen Fensterseite.
    const QList<QAbstractButton *> buttons = view->findChildren<QAbstractButton *>();
    QVERIFY(!buttons.isEmpty());
    const int buttonRight = buttons.first()->geometry().right();
    const int imageLeft = imageLabels.first()->geometry().left();
    QVERIFY2(imageLeft - buttonRight < 40,
             qPrintable(QStringLiteral("Abstand zwischen Auswahl und Bild: %1 px")
                            .arg(imageLeft - buttonRight)));
    delete view;
}

void ExamWidgetTest::clickingAnAnswerImageSelectsIt()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    const Question *question = nullptr;
    for (Question *candidate : catalog.questions()) {
        if (candidate && candidate->id == QStringLiteral("NI104")) {
            question = candidate;
            break;
        }
    }
    QVERIFY(question);

    AnswerView *view = new AnswerView;
    QVector<int> order;
    for (int i = 0; i < question->answers.size(); ++i)
        order.append(i);
    view->setQuestion(question, order, QDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs")));
    view->resize(700, 600);
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));

    QLabel *image = nullptr;
    for (QLabel *label : view->findChildren<QLabel *>()) {
        if (!label->pixmap().isNull()) {
            image = label;
            break;
        }
    }
    QVERIFY2(image, "kein Antwortbild gefunden");
    const QList<QAbstractButton *> buttons = view->findChildren<QAbstractButton *>();
    QVERIFY(!buttons.isEmpty());
    QVERIFY(!buttons.first()->isChecked());

    // Ein Klick auf das Bild muss antworten, sonst trifft man bei
    // Bildfragen nur mit dem Finger auf einen fingerbreiten Punkt.
    QTest::mouseClick(image, Qt::LeftButton, Qt::NoModifier, image->rect().center());
    QVERIFY2(buttons.first()->isChecked(), "Klick auf das Bild hat nicht geantwortet");
    delete view;
}

void ExamWidgetTest::catalogChangeDiscardsTheRunningPaper()
{
    Catalog catalog = loadClassCatalog("de-amateurfunk-N.xml");
    ExamWidget widget;
    widget.setCatalog(&catalog, catalogPathFor("de-amateurfunk-N.xml"));
    QVERIFY(widget.startTraining(5));
    QVERIFY(findLabel(&widget, QStringLiteral("Training —")) != nullptr);

    // A running session keeps a reference to its catalog, so loading another
    // catalog has to drop the paper instead of showing questions of the old
    // one. Afterwards the new catalog can be examined normally.
    Catalog other = loadClassCatalog("de-amateurfunk-A.xml");
    widget.setCatalog(&other, catalogPathFor("de-amateurfunk-A.xml"));

    QVERIFY(findLabel(&widget, QStringLiteral("Training —")) == nullptr);
    QVERIFY(findLabel(&widget, QStringLiteral("1750 Fragen im Katalog verfügbar")) != nullptr);

    QVERIFY(widget.startExam());
    QLabel *header = findLabel(&widget, QStringLiteral("Prüfungsteil V"));
    QVERIFY2(header, qPrintable(QStringLiteral("Kopfzeile nicht gefunden")));
    QVERIFY2(header->text().contains(QStringLiteral("Frage 1 von 125")), qPrintable(header->text()));
}

QTEST_MAIN(ExamWidgetTest)
#include "tst_examwidget.moc"