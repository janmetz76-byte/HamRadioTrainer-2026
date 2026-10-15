// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ExamWidget.h"

#include "core/CatalogXml.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QScrollArea>
#include <QTimer>
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
    void withoutCatalogNothingCanBeStarted();
    void examPresentsTheFirstQuestionOfTheFirstPart();
    void trainingPaperHasNoPassMarkInTheHeader();
    void answersCanBeSelectedAndReplaced();
    void catalogChangeDiscardsTheRunningPaper();
};

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