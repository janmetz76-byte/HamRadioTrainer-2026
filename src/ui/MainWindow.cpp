// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "MainWindow.h"

#include "AppIcons.h"
#include "CatalogView.h"
#include "StartupDialog.h"
#include "ValidationReport.h"

#include "core/CatalogValidator.h"
#include "core/CatalogLocator.h"
#include "core/CatalogXml.h"
#include "ExamWidget.h"
#include "LearningWidget.h"
#include "StatisticsWidget.h"

#include "core/CatalogImporter.h"
#include "core/CatalogManifest.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QDir>
#include <QDateTime>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace hrt {
namespace {

enum Column {
    ColumnTitle = 0,
    ColumnDetail,
    ColumnKind,
};

enum ItemRole {
    QuestionRole = Qt::UserRole + 1,
};

/// Legt die bisherigen Katalogdateien in einem versteckten Unterverzeichnis
/// ab, damit ein Import rueckgaengig gemacht werden kann.
///
/// Der Import ueberschreibt die Dateien im Zielverzeichnis. Handgeaenderte
/// Fragen sind von Hand nicht wiederherstellbar - die Sicherung enthaelt sie
/// deshalb. Der Ordner heisst absichtlich mit Punkt, damit Katalogbrowser und
/// Locator ihn uebersehen.
QString backupCatalogs(const QDir &directory)
{
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QDir target(
        directory.filePath(QStringLiteral(".backup/") + stamp));
    if (!target.mkpath(QStringLiteral(".")))
        return QString();

    for (const QString &name : {QStringLiteral("de-amateurfunk-N.xml"),
                                QStringLiteral("de-amateurfunk-E.xml"),
                                QStringLiteral("de-amateurfunk-A.xml")}) {
        const QString quelle = directory.filePath(name);
        if (QFile::exists(quelle) && !QFile::copy(quelle, target.filePath(name)))
            return QString();
    }
    return target.absolutePath();
}

/// Number of drawable questions of a Prüfungsteil, including its sections.
int partAvailable(const Part &part)
{
    int count = 0;
    for (const Section &section : part.sections)
        count += section.activeQuestionCount();
    return count;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
}

MainWindow::~MainWindow()
{
    // The last paper may not have been followed by another click, so the
    // progress is written once more on the way out.
    saveProgress();
}

void MainWindow::buildUi()
{
    setWindowTitle(tr("HamRadioTrainer 2026"));
    resize(1100, 720);
    // Das Programmsymbol auch im Fenster, nicht nur auf dem Desktop. Es kommt
    // aus den Ressourcen und ist damit auch dann da, wenn der Bearbeitungs-
    // Ablauf kein Icon-Thema findet.
    setWindowIcon(AppIcons::get(QStringLiteral("app")));

    auto *openAction = new QAction(AppIcons::get(QStringLiteral("open")),
                                   tr("&Katalog öffnen…"), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Fragenkatalog öffnen"), QString(),
            tr("Fragenkataloge (*.xml);;Alle Dateien (*)"));
        if (!path.isEmpty())
            openCatalog(path);
    });

    auto *reloadAction = new QAction(AppIcons::get(QStringLiteral("reload")),
                                     tr("A&ktualisieren"), this);
    reloadAction->setShortcut(QKeySequence::Refresh);
    connect(reloadAction, &QAction::triggered, this,
            [this] { if (!m_catalogPath.isEmpty()) openCatalog(m_catalogPath); });

    auto *resetProgressAction =
        new QAction(AppIcons::get(QStringLiteral("reset-progress")),
                    tr("Lern&fortschritt zurücksetzen"), this);
    connect(resetProgressAction, &QAction::triggered, this, [this] {
        if (m_catalogPath.isEmpty())
            return;
        const auto answer = QMessageBox::question(
            this, tr("Lernfortschritt zurücksetzen"),
            tr("Alle Lernstufen, Fälligkeiten und Trefferquoten dieses Katalogs werden "
               "gelöscht. Die Fragenkataloge selbst bleiben unverändert. Wirklich löschen?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        QString error;
        if (m_progressStore.clear(m_catalog, &error)) {
            m_exam->clearProgress();
            m_learning->clearProgress();
        m_catalogView->setProgress({});
        m_statistics->setProgress({});
            m_status->setText(tr("Lernfortschritt zurückgesetzt"));
        } else {
            QMessageBox::warning(this, tr("Zurücksetzen fehlgeschlagen"), error);
        }
    });

    auto *quitAction = new QAction(AppIcons::get(QStringLiteral("quit")),
                                   tr("&Beenden"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *fileMenu = menuBar()->addMenu(tr("&Datei"));
    // Der Assistent ist nicht nur beim ersten Start nuetzlich: Wer sich
    // zwischenzeitlich fuer eine andere Klasse entscheidet, oeffnet ihn ueber
    // das Menue erneut.
    auto *assistantAction = new QAction(AppIcons::get(QStringLiteral("assistant")),
                                        tr("&Einrichtungsassistent…"), this);
    connect(assistantAction, &QAction::triggered, this, [this] {
        QSettings settings;
        settings.setValue(QStringLiteral("startup/done"), false);
        runStartupAssistant();
    });
    fileMenu->addAction(assistantAction);
    fileMenu->addSeparator();
    fileMenu->addAction(openAction);
    fileMenu->addAction(reloadAction);
    fileMenu->addSeparator();

    // Die Aktualisierung steht bei den Katalogdateien und nicht in einem
    // eigenen Menü: sie schreibt genau die Dateien, die "Katalog öffnen" liest.
    auto *updateAction = new QAction(AppIcons::get(QStringLiteral("update")),
                                      tr("Fragenkatalog &aktualisieren…"), this);
    updateAction->setToolTip(
        tr("Fragenkatalog der Bundesnetzagentur neu einlesen. Wählt das Verzeichnis mit "
           "fragenkatalog3b.json aus dem geladenen Archiv."));
    connect(updateAction, &QAction::triggered, this, [this] { updateCatalog(); });
    fileMenu->addAction(updateAction);
    m_updateAction = updateAction;

    fileMenu->addSeparator();
    fileMenu->addAction(resetProgressAction);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);

    // Der Pruefbericht ist ein andockbares Panel und kein Dialog: die
    // mitgelieferten Kataloge erzeugen mehrere hundert Hinweise, die den
    // Start nicht unterbrechen duerfen.
    m_report = new ValidationReport(this);
    m_report->setHeadline(
        tr("Prüfbericht: Was steckt in diesem Fragenkatalog? Geprüft wird die "
           "Katalogdatei auf Fehler und auf Hinweise — fehlende Bilder, doppelte "
           "Kennungen, unvollständige Punktewerte, fehlende Quellenangaben. Ein Fehler "
           "macht den Katalog unbrauchbar, ein Hinweis nicht."));
    m_reportDock = new QDockWidget(tr("Prüfbericht"), this);
    m_reportDock->setObjectName(QStringLiteral("validationReport"));
    m_reportDock->setWidget(m_report);
    addDockWidget(Qt::BottomDockWidgetArea, m_reportDock);
    m_reportDock->hide();

    auto *reportAction = m_reportDock->toggleViewAction();
    reportAction->setIcon(AppIcons::get(QStringLiteral("report")));
    reportAction->setText(tr("Prüf&bericht anzeigen"));
    reportAction->setToolTip(
        tr("Prüft den geladenen Fragenkatalog und listet Fehler und Hinweise auf. "
           "Ein Fehler macht den Katalog unbrauchbar."));
    m_showReportAction = reportAction;

    // Der Wegweiser des Berichts stand früher als freistehender Punkt auf der
    // Menüleiste: er sagte nicht, was er prüft, und seine Bedienbefehle waren
    // nur zu finden, wenn man das Panel schon offen hatte. Als eigenes Menü
    // mit Namen sagt er mehr und trägt die Befehle dort, wo man sie sucht.
    QMenu *reportMenu = menuBar()->addMenu(tr("&Prüfbericht"));
    reportMenu->addAction(m_showReportAction);
    reportMenu->addSeparator();

    m_recheckAction = reportMenu->addAction(AppIcons::get(QStringLiteral("recheck")),
                                            tr("Katalog &erneut prüfen"));
    m_recheckAction->setToolTip(
        tr("Prüft den geladenen Katalog noch einmal und schreibt den Bericht neu."));
    connect(m_recheckAction, &QAction::triggered, this,
            [this] { if (!m_catalogPath.isEmpty()) openCatalog(m_catalogPath); });

    m_copyReportAction = reportMenu->addAction(AppIcons::get(QStringLiteral("copy")),
                                               tr("Bericht in die Zwischenablage"));
    m_copyReportAction->setToolTip(
        tr("Kopiert den vollständigen Bericht als Text in die Zwischenablage."));
    connect(m_copyReportAction, &QAction::triggered, this,
            [this] { m_report->copyToClipboard(); });
    updateReportActions();

    QMenu *helpMenu = menuBar()->addMenu(tr("&Hilfe"));
    auto *aboutAction = new QAction(AppIcons::get(QStringLiteral("about")),
                                    tr("&Über HamRadioTrainer 2026"), this);
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("Über HamRadioTrainer 2026"));
        box.setText(tr("<h3>HamRadioTrainer 2026</h3>"
                       "<p>Neu aufgebaut von Jan - DC6DX.</p>"
                       "<p>Lizenz: GNU General Public License v3 oder später.</p>"
                       "<p>Fragenkataloge liegen als unverschlüsseltes XML vor und können "
                       "direkt bearbeitet werden.</p>"));
        // Die Herkunft der Fragen gehoert in die Hilfe und nicht an jede
        // Zeile der Oberflaeche: in der Auswahl der Pruefungsteile steht sie
        // nur im Weg.
        box.setInformativeText(catalogProvenance());
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
    });
    helpMenu->addAction(aboutAction);

    // Ansichtsleiste links, Arbeitsbereich rechts - so ist es im Original
    // von 2015 aufgebaut. Reiter waren fuer diese Anwendung zu gedrängt:
    // man konnte nie sehen, was es sonst gibt.
    // Die Seitenleiste ist eine Leiste mit Namen, nicht nur eine Liste von
    // Knoepfen: mit Kopfzeile und Gruppenueberschrift, wie im Original von
    // 2015. Ohne Kopf weiss niemand, was die Knöpfe links sind, und ohne
    // Gruppenueberschrift sieht eine Liste mit einem Eintrag unfertig aus.
    auto *rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("ansichtsleiste"));
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(6, 4, 6, 6);
    railLayout->setSpacing(4);

    auto *kopf = new QWidget(rail);
    auto *kopfLayout = new QHBoxLayout(kopf);
    kopfLayout->setContentsMargins(2, 0, 0, 0);
    kopfLayout->setSpacing(4);
    auto *kopfTitel = new QLabel(tr("Menü"), kopf);
    QFont kopfSchrift = kopfTitel->font();
    kopfSchrift.setBold(true);
    kopfTitel->setFont(kopfSchrift);
    kopfLayout->addWidget(kopfTitel);
    kopfLayout->addStretch(1);

    // Zusammenklappen: die Ansicht soll Platz bekommen, wenn jemand eine
    // lange Tabelle liest. Der Knopf bleibt sichtbar - sonst waere die
    // Leiste weg und nicht wiederzufinden.
    auto *einklappen = new QToolButton(kopf);
    einklappen->setObjectName(QStringLiteral("leisteEinklappen"));
    einklappen->setIcon(AppIcons::get(QStringLiteral("collapse")));
    einklappen->setToolTip(tr("Ansichtsleiste einklappen"));
    einklappen->setAutoRaise(true);
    kopfLayout->addWidget(einklappen);
    railLayout->addWidget(kopf);

    auto *gruppenKopf = new QLabel(tr("Lernen"), rail);
    gruppenKopf->setObjectName(QStringLiteral("leisteGruppenkopf"));
    gruppenKopf->setAlignment(Qt::AlignCenter);
    QFont gruppenSchrift = gruppenKopf->font();
    gruppenSchrift.setBold(true);
    gruppenSchrift.setPointSizeF(gruppenSchrift.pointSizeF() - 1.0);
    gruppenKopf->setFont(gruppenSchrift);
    railLayout->addWidget(gruppenKopf);

    auto *knopfBereich = new QWidget(rail);
    auto *knopfLayout = new QVBoxLayout(knopfBereich);
    knopfLayout->setContentsMargins(0, 0, 0, 0);
    knopfLayout->setSpacing(6);

    m_viewGroup = new QButtonGroup(rail);
    m_viewGroup->setExclusive(true);

    m_catalogView = new CatalogView(this);
    m_exam = new ExamWidget(this);
    m_learning = new LearningWidget(this);
    m_statistics = new StatisticsWidget(this);

    // Vier feste Ansichten mit festem Bild. Die Bilder gehoeren zum Icon-Satz
    // des Programms, nicht zum Desktop-Thema: sonst sieht dieselbe
    // Anwendung auf einem Rechner aus wie auf dem anderen.
    struct Ansicht
    {
        QString icon;
        QString title;
        QWidget *widget;
    };
    const QVector<Ansicht> views = {
        {QStringLiteral("view-catalog"), tr("Katalogansicht"), m_catalogView},
        {QStringLiteral("view-learn"), tr("Fragen lernen"), m_learning},
        {QStringLiteral("view-exam"), tr("Prüfungssimulation"), m_exam},
        {QStringLiteral("view-statistics"), tr("Statistik"), m_statistics},
    };

    for (int i = 0; i < views.size(); ++i) {
        // Werkzeugknopf statt Rundknopf: Qt zeichnet das Bild eines
        // QRadioButton nicht - nur der Punkt und die Beschriftung. Der
        // Umschalter erfuellt denselben Zweck und zeigt endlich das Bild.
        // Bild ueber Text, mittig: der Knopf wird als Schaltflaeche erkannt und
        // nicht als Beschriftung. Neben dem Bild waere die Zeile zweispaltig zu
        // lesen, weil das Auge erst die Beschriftung suchen muss.
        auto *button = new QToolButton(knopfBereich);
        button->setText(views.at(i).title);
        button->setIcon(AppIcons::get(views.at(i).icon));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(36, 36));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolTip(views.at(i).title);
        // Der Objektname macht den Umschalter von aussen auffindbar - fuer
        // den Test und fuer jeden, der die Ansicht umschalten will, ohne
        // sich durch die Oberflaeche zu tasten.
        button->setObjectName(QStringLiteral("ansicht%1").arg(i));
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        button->setMinimumSize(QSize(136, 86));
        m_viewGroup->addButton(button, i);
        if (i == 0)
            button->setChecked(true);
        knopfLayout->addWidget(button);
        m_views.append(views.at(i).widget);
    }
    knopfLayout->addStretch(1);
    railLayout->addWidget(knopfBereich);

    // Der gewaehlte Punkt bekommt einen Rahmen, wie im Original. Ohne ihn ist
    // bei drei Knöpfen auf Anhieb nicht zu sehen, welche Ansicht laeuft.
    rail->setStyleSheet(QStringLiteral(
        "QToolButton[objectName^=\"ansicht\"] {"
        "  border: 1px solid transparent; border-radius: 4px; padding: 4px;"
        "}"
        "QToolButton[objectName^=\"ansicht\"]:checked {"
        "  border: 1px solid palette(highlight); background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"));

    // Der Zustand gehoert an den Knopf, nicht in eine Variable dieser
    // Funktion: die Lambda laeuft weiter, lange nachdem buildUi zurueckgekehrt
    // ist, und wuerde dann in einen zerstoerten Stack-Zustand schreiben.
    // Der Knopf ist zudem der einzige, der ihn ohnehin kennt.
    einklappen->setCheckable(true);
    connect(einklappen, &QToolButton::clicked, this, [rail, knopfBereich, gruppenKopf,
                                                      kopfTitel, einklappen] {
        const bool eingeklappt = einklappen->isChecked();
        knopfBereich->setVisible(!eingeklappt);
        gruppenKopf->setVisible(!eingeklappt);
        kopfTitel->setVisible(!eingeklappt);
        rail->setFixedWidth(eingeklappt ? 48 : 156);
        einklappen->setIcon(AppIcons::get(eingeklappt ? QStringLiteral("expand")
                                                      : QStringLiteral("collapse")));
        einklappen->setToolTip(eingeklappt ? tr("Ansichtsleiste ausklappen")
                                           : tr("Ansichtsleiste einklappen"));
    });
    rail->setFixedWidth(156);

    connect(m_viewGroup, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton *button) { selectView(m_viewGroup->id(button)); });

    auto *body = new QStackedWidget(this);
    for (QWidget *view : m_views)
        body->addWidget(view);
    m_viewStack = body;

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(rail);
    splitter->addWidget(body);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({190, 1100});
    setCentralWidget(splitter);

    connect(m_exam, &ExamWidget::progressChanged, this, [this] { saveProgress(); });
    connect(m_learning, &LearningWidget::progressChanged, this, [this] { saveProgress(); });
    connect(m_exam, &ExamWidget::examFinished, this, &MainWindow::showResult);

    m_status = new QLabel(tr("Kein Katalog geladen"), this);
    statusBar()->addWidget(m_status, 1);
    m_examDateLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_examDateLabel);
    updateExamCountdown();

}

/// Woher die Fragen des geladenen Katalogs stammen.
///
/// Aus den Metadaten der Datei, nicht fest im Programm: ein selbst
/// gebauter oder von Hand ergaenzter Katalog nennt seine eigene Herkunft,
/// und die Anzeige bleibt trotzdem wahr. Ohne Katalog bleibt nur der
/// Hinweis, dass keine Fragen geladen sind - eine Herkunft zu erfinden waere
/// hier besonders falsch.
QString MainWindow::catalogProvenance() const
{
    if (m_catalogPath.isEmpty())
        return tr("Es ist kein Fragenkatalog geladen. Die Herkunft der Fragen steht "
                  "in den Metadaten des Katalogs und erscheint hier, sobald einer "
                  "geladen ist.");

    const CatalogMetadata &metadata = m_catalog.metadata;
    QStringList absaetze;

    // "Fragenbasis" zuerst: sie beantwortet die Frage, die hier gestellt wird.
    if (!metadata.attribution.isEmpty())
        absaetze.append(tr("Fragenbasis: %1").arg(metadata.attribution));
    if (!metadata.authority.isEmpty())
        absaetze.append(tr("Herausgeber: %1").arg(metadata.authority));

    // Die Lizenz zweimal, weil sie zweimal gebraucht wird: im Klartext, damit
    // man weiss, was erlaubt ist, und als Kennung, damit man sie nachschlagen
    // kann.
    QStringList lizenz;
    if (!metadata.licenseName.isEmpty())
        lizenz.append(tr("Lizenz Fragenkatalog: %1").arg(metadata.licenseName));
    if (!metadata.license.isEmpty())
        lizenz.append(tr("Datenlizenz: %1").arg(metadata.license));
    if (!lizenz.isEmpty()) {
        absaetze.append(QString());
        absaetze.append(lizenz.join(QLatin1Char('\n')));
    }

    return absaetze.join(QStringLiteral("\n\n"));
}

/// Reads the learning progress that belongs to this catalog. A progress file
/// that belongs to an earlier revision of the catalog is not applied, because
/// the questions may have been reworded or replaced by hand.
/// Shows the graded paper: the points of every Prüfungsteil against its own
/// pass mark, and which parts have to be repeated orally.
void MainWindow::showResult(const ExamResult &result)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Ergebnis der Prüfung"));
    dialog.resize(780, 480);

    auto *layout = new QVBoxLayout(&dialog);

    auto *headline = new QLabel(&dialog);
    headline->setWordWrap(true);
    if (result.passed) {
        headline->setText(tr("<b>Bestanden.</b> %1 von %2 Punkten.")
                              .arg(result.achievedPoints)
                              .arg(result.maxPoints));
    } else {
        headline->setText(tr("<b>Nicht bestanden.</b> %1 von %2 Punkten. Bestehen je "
                             "Prüfungsteil ab %3 Punkten.")
                              .arg(result.achievedPoints)
                              .arg(result.maxPoints)
                              .arg(result.parts.isEmpty() ? 19 : result.parts.first().passScore));
    }
    layout->addWidget(headline);

    if (!result.passed && !result.parts.isEmpty() && !result.failedParts().isEmpty()) {
        auto *hint = new QLabel(&dialog);
        hint->setWordWrap(true);
        hint->setText(tr("Nicht bestanden: Prüfungsteil %1. Dieser Teil ist mündlich "
                         "nachzuprüfen; die übrigen Teile müssen nicht wiederholt werden.")
                          .arg(result.failedParts().join(QStringLiteral(", "))));
        layout->addWidget(hint);
    }

    auto *table = new QTableWidget(&dialog);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({tr("Prüfungsteil"), tr("Punkte"), tr("Bestehen"),
                                      tr("falsch"), tr("offen")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (const PartResult &part : result.parts) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0,
                       new QTableWidgetItem(QStringLiteral("%1 — %2").arg(part.letter, part.title)));
        table->setItem(row, 1, new QTableWidgetItem(QStringLiteral("%1 / %2")
                                                        .arg(part.achievedPoints)
                                                        .arg(part.maxPoints)));
        table->setItem(row, 2, new QTableWidgetItem(QString::number(part.passScore)));
        table->setItem(row, 3, new QTableWidgetItem(QString::number(part.wrongAnswers)));
        table->setItem(row, 4, new QTableWidgetItem(QString::number(part.unanswered)));
        if (!part.passed()) {
            for (int column = 0; column < table->columnCount(); ++column)
                table->item(row, column)->setBackground(QColor(0xff, 0xe0, 0xe0));
        }
    }
    table->resizeColumnsToContents();
    layout->addWidget(table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::loadProgress()
{
    m_progressStore = ProgressStore(ProgressStore::defaultPath(m_catalog));
    const ProgressLoad loaded = m_progressStore.load(m_catalog);

    if (!loaded.error.isEmpty()) {
        m_status->setText(tr("Fortschritt nicht geladen: %1").arg(loaded.error));
        return;
    }
    if (loaded.catalogChanged) {
        // Der Katalog hat sich geaendert. Das ist kein Grund, jemandes
        // Arbeit wegzuwerfen - die Frage ist nur, welche der gespeicherten
        // Staende noch zu den Fragen gehoeren, die jetzt dastehen.
        QString archiveError;
        if (!m_progressStore.archive(&archiveError))
            m_status->setText(tr("Fortschritt nicht gesichert: %1").arg(archiveError));

        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(tr("Fragenkatalog geändert"));
        box.setText(tr("Seit deinem letzten Lernen wurde der Fragenkatalog geändert. "
                       "Dein Lernfortschritt ist noch da."));

        QStringList details;
        if (loaded.keptStates > 0) {
            details.append(tr("%1 Lernstände gehören zu unveränderten Fragen und werden "
                              "übernommen.")
                               .arg(loaded.keptStates));
        }
        if (loaded.resetStates > 0) {
            details.append(tr("%1 Lernstände gehören zu Fragen, die es noch gibt, aber "
                              "anders formuliert sind. Sie zählen neu, denn die alte "
                              "Antwort galt für einen anderen Wortlaut.")
                               .arg(loaded.resetStates));
        }
        if (loaded.droppedStates > 0) {
            details.append(tr("%1 Lernstände gehören zu Fragen, die es nicht mehr gibt; "
                              "sie werden verworfen.")
                               .arg(loaded.droppedStates));
        }
        if (loaded.withoutFingerprints) {
            details.append(tr("Ein Teil des Fortschritts stammt aus einer älteren "
                              "Programmversion. Für diese Einträge ließ sich nur die "
                              "Fragennummer vergleichen, nicht der Wortlaut."));
        }
        details.append(tr("Die bisherige Datei bleibt als Sicherung neben der neuen "
                          "liegen (%1).")
                           .arg(QFileInfo(m_progressStore.path()).absolutePath()
                                + QStringLiteral("/")));
        box.setInformativeText(details.join(QLatin1Char('\n')));

        auto *behalten = box.addButton(tr("Fortschritt übernehmen"),
                                       QMessageBox::AcceptRole);
        auto *neuBeginnen = box.addButton(tr("Neu beginnen"),
                                          QMessageBox::DestructiveRole);
        box.setDefaultButton(behalten);
        box.exec();

        // Nur ein ausdruecklicher Klick auf "Neu beginnen" verwirft. Wer das
        // Fenster schliesst oder Escape drueckt, hat sich nicht entschieden -
        // und eine unbeabsichtigte Entscheidung darf keinen Fortschritt kosten.
        if (box.clickedButton() == neuBeginnen) {
            // Auch die Datei wird geleert und neu gebunden. Ließe man sie
            // stehen, naeme derselbe Dialog beim naechsten Start denselben
            // Stand wieder vor - der Wunsch waere nie angekommen.
            QString clearError;
            if (!m_progressStore.clear(m_catalog, &clearError)) {
                m_status->setText(tr("Fortschritt nicht geleert: %1").arg(clearError));
            } else {
                m_status->setText(tr("Lernfortschritt neu begonnen"));
            }
            m_exam->clearProgress();
            m_exam->setProgress({});
            m_learning->clearProgress();
            m_catalogView->setProgress({});
            m_statistics->setProgress({});
            return;
        }
    }
    if (loaded.droppedStates > 0) {
        m_status->setText(tr("%1 Lernstände zu entfernten Fragen wurden verworfen")
                              .arg(loaded.droppedStates));
    }
    m_exam->setProgress(loaded.states);
    m_learning->setProgress(loaded.states);
    m_catalogView->setProgress(loaded.states);
    m_statistics->setProgress(loaded.states);

    // Nach einem Katalogwechsel wird sofort neu gespeichert. Sonst staende die
    // Datei weiterhin auf dem alten Katalogeinstand, und dieselbe Frage tauchte
    // bei jedem Start erneut auf. Nebeneffekt und eigentlicher Zweck: die
    // Fingerabdruecke der Fragen kommen erst jetzt in die Datei.
    if (loaded.catalogChanged)
        saveProgress();
}

void MainWindow::saveProgress()
{
    if (m_catalogPath.isEmpty())
        return;
    const QVector<QuestionState> states = m_exam->scheduler().states();
    QString error;
    if (!m_progressStore.save(m_catalog, states, &error)) {
        m_status->setText(tr("Fortschritt nicht gespeichert: %1").arg(error));
        return;
    }
    // Die Fragenliste zeigt Lernstufe und Wiedervorlage; ohne diese Zeile
    // bliebe sie nach jeder Runde stehen.
    m_catalogView->setProgress(states);
    m_statistics->setProgress(states);
}

void MainWindow::rememberCatalog(const QString &path)
{
    QSettings settings;
    settings.setValue(QStringLiteral("catalog/lastPath"), path);
}

void MainWindow::runStartupAssistant()
{
    // Der Startassistent laeuft nur beim ersten Start. Danach ist die Wahl
    // gemerkt und der Weg in die Anwendung fuehrt direkt zur Arbeit.
    QSettings settings;
    if (settings.value(QStringLiteral("startup/done")).toBool())
        return;

    const CatalogLocator::Result located = CatalogLocator::locate();
    if (located.paths.isEmpty())
        return;

    StartupDialog dialog(located.paths, this);
    if (dialog.exec() != QDialog::Accepted || !dialog.result().accepted)
        return;

    const StartupDialog::Result chosen = dialog.result();
    settings.setValue(QStringLiteral("startup/done"), true);
    settings.setValue(QStringLiteral("startup/examDate"), chosen.examDate);
    QVariantList parts;
    for (int part : chosen.selectedParts)
        parts.append(part);
    settings.setValue(QStringLiteral("startup/parts"), parts);

    if (!chosen.catalogPath.isEmpty() && chosen.catalogPath != m_catalogPath)
        openCatalog(chosen.catalogPath);
    m_exam->setSelectedParts(chosen.selectedParts);
    // Der Assistent hat den Termin nur in die Einstellungen geschrieben. Ohne
    // dieses Zuspieln blieb die Countdown-Anzeige die ganze Sitzung ueber leer,
    // obwohl der Termin eingetragen war.
    m_examDate = chosen.examDate;
    updateExamCountdown();

    // Der Assistent fragt "Wofuer moechten Sie lernen?" - die Antwort ist
    // eine Lernaufgabe, kein Katalogverzeichnis. Wer danach in der
    // Katalogansicht landet, muss sich noch einmal durchklicken, bis er
    // anfangen kann.
    selectView(kLearningView);
}

void MainWindow::updateExamCountdown()
{
    const QDate date = m_examDate;
    if (!date.isValid()) {
        m_examDateLabel->setText(tr("Kein Prüfungstermin hinterlegt"));
        return;
    }
    const qint64 days = QDate::currentDate().daysTo(date);
    if (days < 0)
        m_examDateLabel->setText(tr("Prüfungstermin war am %1").arg(date.toString(Qt::ISODate)));
    else if (days == 0)
        m_examDateLabel->setText(tr("Heute ist Prüfungstag"));
    else
        m_examDateLabel->setText(tr("Noch %1 Tage bis zur Prüfung").arg(days));
}

void MainWindow::openInitialCatalog()
{
    // The catalog of last use, as long as it is still there. A hand edited
    // catalog keeps its path, so the learner's own questions are not lost.
    QSettings settings;
    const QString last = settings.value(QStringLiteral("catalog/lastPath")).toString();
    // Von einem vorherigen Start bekannte Auswahl uebernehmen, bevor der
    // Assistent neu fragt.
    m_examDate = settings.value(QStringLiteral("startup/examDate")).toDate();
    const QVariantList storedParts = settings.value(QStringLiteral("startup/parts")).toList();
    if (!storedParts.isEmpty()) {
        QSet<int> parts;
        for (const QVariant &value : storedParts)
            parts.insert(value.toInt());
        m_exam->setSelectedParts(parts);
    }
    updateExamCountdown();

    bool opened = false;
    if (!last.isEmpty() && QFileInfo::exists(last))
        opened = openCatalog(last);

    if (!opened) {
        const CatalogLocator::Result located = CatalogLocator::locate();
        if (located.paths.isEmpty()) {
            m_status->setText(located.problem);
            return;
        }
        opened = openCatalog(located.paths.first());
    }
    if (!opened)
        return;

    // Der Assistent laeuft unabhaengig davon, woher der Katalog kam. Ein
    // frueherer Rueckkehrpfad hat ihn genau deshalb bei jedem zweiten Start
    // uebersprungen - bei frisch Installiertem war er nie zu sehen, und bei
    // denen, die das Programm schon benutzt hatten, nur nach einem
    // Katalogwechsel.
    runStartupAssistant();
}

bool MainWindow::openCatalog(const QString &path)
{
    Catalog catalog;
    QString error;
    if (!CatalogXml::load(path, &catalog, &error)) {
        QMessageBox::critical(this, tr("Katalog konnte nicht geladen werden"), error);
        m_status->setText(tr("Fehler: %1").arg(error));
        return false;
    }

    QVector<CatalogIssue> issues;
    // Der Validator rechnet die Bildpfade relativ zu einem Verzeichnis, nicht
    // relativ zur Katalogdatei. Uebergibt man hier den Dateipfad, meldet er
    // jedes einzelne Bild als fehlend.
    const bool usable =
        CatalogValidator::validate(catalog, &issues, QFileInfo(path).absolutePath());

    m_catalog = catalog;
    m_catalogPath = path;
    rememberCatalog(path);
    updateReportActions();
    m_catalogView->setCatalog(&m_catalog, path);
    m_exam->setCatalog(&m_catalog, path);
    m_learning->setCatalog(&m_catalog, path);
    m_statistics->setCatalog(&m_catalog, path);
    loadProgress();
    updateStatus();

    if (!usable) {
        refreshReport(issues);
        return false;
    }

    int warnings = 0;
    for (const CatalogIssue &issue : issues) {
        if (issue.severity == IssueSeverity::Warning)
            ++warnings;
    }
    // Ohne Beanstandungen wird der Bericht trotzdem gefuellt. Er soll nach
    // einem Start auch im leeren Zustand sagen, was er geprueft hat - sonst
    // bleibt unklar, ob "nichts zu sehen" geprueft oder nur nicht geprueft
    // bedeutet.
    refreshReport(issues);
    if (warnings == 0 && m_report->errorCount() == 0 && m_report->warningCount() == 0)
        m_reportDock->hide();
    return true;
}

void MainWindow::selectView(int index)
{
    if (index < 0 || index >= m_views.size())
        return;
    m_viewStack->setCurrentIndex(index);
    // Die Ansichtsleiste ist die Bedienung, der Stapel nur ihre Anzeige.
    // Wer nur den Stapel setzt, laesst die Leiste auf der alten Ansicht
    // stehen - dann beantwortet der naechste Klick etwas anderes als
    // angezeigt.
    if (QAbstractButton *button = m_viewGroup->button(index))
        button->setChecked(true);
}


void MainWindow::updateStatus()
{
    if (m_catalogPath.isEmpty()) {
        m_status->setText(tr("Kein Katalog geladen"));
        return;
    }
    m_status->setText(tr("%1 — Klasse %2, Prüfung %3 — %4 Fragen gesamt, %5 verfügbar")
                          .arg(QFileInfo(m_catalogPath).fileName(),
                               examClassToString(m_catalog.metadata.examClass),
                               m_catalog.metadata.examId)
                          .arg(m_catalog.totalQuestionCount())
                          .arg(m_catalog.activeQuestionCount()));
}

void MainWindow::updateReportActions()
{
    // Ohne Katalog gibt es nichts zu pruefen. Die Eintraege bleiben sichtbar
    // und werden nur blass, damit das Menü nicht bei jedem Katalogwechsel
    // seine Gestalt aendert.
    const bool loaded = !m_catalogPath.isEmpty();
    m_recheckAction->setEnabled(loaded);
    m_copyReportAction->setEnabled(loaded);
    // Ohne geladenen Katalog gibt es kein Verzeichnis, in das ein Import
    // schreiben koennte.
    if (m_updateAction)
        m_updateAction->setEnabled(loaded);
}

void MainWindow::refreshReport(const QVector<CatalogIssue> &validatorIssues)
{
    // Der Validator beantwortet "Ist dieser Katalog benutzbar?". Der Sollstand
    // beantwortet eine andere Frage: "Ist das noch der Katalog, den das
    // Programm ausgeliefert hat?" Beides gehoert in denselben Bericht, weil
    // sich beides im selben Fenster zeigt.
    const QString directory =
        m_catalogPath.isEmpty() ? QString() : QFileInfo(m_catalogPath).absolutePath();
    const CatalogManifest manifest = CatalogManifest::load(directory);
    const CatalogDrift drift = manifest.driftOf(m_catalog, directory);

    QVector<CatalogIssue> issues = validatorIssues;
    CatalogManifestEntry own;
    for (const CatalogManifestEntry &candidate : manifest.entries()) {
        if (candidate.examClass == examClassToString(m_catalog.metadata.examClass))
            own = candidate;
    }

    const QString text = CatalogManifest::describe(drift, m_catalog, own);
    if (drift != CatalogDrift::AsShipped) {
        // Ein Hinweis, kein Fehler: die Katalogdatei ist benutzbar, nur eben
        // nicht mehr die ausgelieferte.
        CatalogIssue issue;
        issue.severity = IssueSeverity::Warning;
        issue.location = tr("Katalogeinstand");
        issue.message = text;
        issues.append(issue);
    }

    switch (drift) {
    case CatalogDrift::AsShipped:
        m_report->setHeadline(tr("Katalog geprüft – er entspricht dem ausgelieferten Stand."));
        break;
    case CatalogDrift::Edited:
        m_report->setHeadline(tr("Katalog geprüft – er wurde von Hand geändert."));
        break;
    case CatalogDrift::Newer:
        m_report->setHeadline(tr("Katalog geprüft – er ist neuer als ausgeliefert."));
        break;
    case CatalogDrift::Unknown:
        m_report->setHeadline(tr("Katalog geprüft."));
        break;
    }

    showIssues(issues);
}

void MainWindow::updateCatalog()
{
    if (m_catalogPath.isEmpty())
        return;

    const QDir target(QFileInfo(m_catalogPath).absolutePath());
    QSettings settings;
    const QString previous =
        settings.value(QStringLiteral("catalog/updateSourceDirectory")).toString();

    const QString source = QFileDialog::getExistingDirectory(
        this,
        tr("Verzeichnis mit dem Archiv der Bundesnetzagentur wählen"),
        previous.isEmpty() ? QDir::homePath() : previous);
    if (source.isEmpty())
        return;

    QString error;
    if (!CatalogImporter::sourceIsUsable(source, &error)) {
        QMessageBox::critical(this, tr("Archiv nicht verwendbar"), error);
        return;
    }
    settings.setValue(QStringLiteral("catalog/updateSourceDirectory"), source);

    // Die Warnung ist nicht theoretisch: nach dem Import stimmt die
    // Pruefsumme der Datei nicht mehr, und der an den Katalogeinstand
    // gebundene Lernfortschritt wird dann bewusst verworfen.
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("Fragenkatalog aktualisieren"));
    box.setText(tr("Der Import schreibt alle drei Prüfungsklassen und ihre Bilder "
                   "nach %1.").arg(target.absolutePath()));
    box.setInformativeText(
        tr("Handgeänderte Fragen in diesem Verzeichnis gehen dabei verloren; der "
           "vorherige Stand wird gesichert. Ändert sich der Katalog, wird der "
           "Lernfortschritt zurückgesetzt, weil er zu den alten Fragen gehört."));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);
    if (box.exec() != QMessageBox::Yes)
        return;

    // Die Sicherung entsteht vor dem ersten geschriebenen Byte. Sie kostet
    // nichts und ist der einzige Weg zurueck, wenn der Import abbricht.
    const QString sicherung = backupCatalogs(target);
    if (sicherung.isEmpty()) {
        QMessageBox::critical(this, tr("Sicherung nicht möglich"),
                              tr("Der Import würde die vorhandenen Katalogdateien "
                                 "unwiderruflich ersetzen. Bitte den Ordner "
                                 "%1 beschreibbar machen und es erneut versuchen.")
                                  .arg(target.absolutePath()));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const CatalogImportReport report = CatalogImporter::importSource(source, target.absolutePath());
    QApplication::restoreOverrideCursor();

    QMessageBox result(this);
    result.setWindowTitle(tr("Fragenkatalog aktualisieren"));
    if (report.ok) {
        result.setIcon(QMessageBox::Information);
        result.setText(tr("Import abgeschlossen."));
    } else {
        result.setIcon(QMessageBox::Warning);
        result.setText(tr("Import abgeschlossen, aber mit Fehlern."));
    }

    QStringList lines;
    for (const CatalogImportEntry &entry : report.catalogs)
        lines.append(tr("%1: %2 Fragen").arg(entry.fileName).arg(entry.questions));
    lines.append(tr("Bilder kopiert: %1, fehlend: %2")
                     .arg(report.imagesCopied)
                     .arg(report.imagesMissing));
    lines.append(tr("Vorheriger Stand gesichert unter %1").arg(sicherung));
    if (!report.errors.isEmpty())
        lines.append(QString());
    for (const QString &message : report.errors)
        lines.append(tr("Fehler: %1").arg(message));
    for (const QString &message : report.warnings)
        lines.append(tr("Hinweis: %1").arg(message));
    result.setInformativeText(lines.join(QLatin1Char('\n')));
    result.setDetailedText(report.errors.join(QLatin1Char('\n')));
    result.exec();

    if (!report.ok)
        return;

    // Der geladene Katalog liegt jetzt neu vor: neu laden, damit Fragen,
    // Zaehlung und Pruefbericht zum importierten Stand gehoeren.
    const QString file =
        QStringLiteral("de-amateurfunk-%1.xml").arg(examClassToString(m_catalog.metadata.examClass));
    openCatalog(target.filePath(file));
}

void MainWindow::showIssues(const QVector<CatalogIssue> &issues)
{
    m_report->setIssues(issues);

    const int errors = m_report->errorCount();
    const int warnings = m_report->warningCount();
    if (errors > 0) {
        // Ohne diese Meldung merkt niemand, dass der Katalog unbrauchbar ist.
        QMessageBox box(this);
        box.setIcon(QMessageBox::Critical);
        box.setWindowTitle(tr("Katalog enthält Fehler"));
        box.setText(tr("%1 Fehler, %2 Hinweise.").arg(errors).arg(warnings));
        box.setInformativeText(
            tr("Der Katalog wird nicht zum Lernen oder Prüfen verwendet. Die vollständige "
               "Liste steht im Prüfbericht unten."));
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
        m_reportDock->show();
        return;
    }

    if (warnings > 0) {
        // Hinweise blockieren nicht. Der Zähler im Menü weist auf sie hin.
        m_reportDock->setWindowTitle(
            tr("Prüfbericht (%1)").arg(warnings == 1 ? tr("1 Hinweis")
                                                    : tr("%1 Hinweise").arg(warnings)));
    }
}

} // namespace hrt

#include "MainWindow.moc"