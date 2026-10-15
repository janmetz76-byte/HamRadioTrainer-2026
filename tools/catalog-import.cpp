// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Imports the machine readable question catalog of the Bundesnetzagentur
// (fragenkatalog3b.json plus its SVG image directory) into the plain XML
// catalog format of this project, one catalog per examination class.
//
// The question text is not covered by the GPL. It is redistributed under
// DL-DE-BY-2.0, which requires the verbatim source notice; the importer writes
// that notice into the metadata of every generated catalog and marks the
// catalog as unmodified.
#include "core/Catalog.h"
#include "core/CatalogValidator.h"
#include "core/CatalogXml.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

using namespace hrt;

namespace {

/// Mandatory source notice, taken verbatim from the README of the source.
const QString kAttribution =
    QStringLiteral("Prüfungsfragen zum Erwerb von Amateurfunkprüfungsbescheinigungen, "
                   "Bundesnetzagentur, 3. Auflage, März 2024, "
                   "(www.bundesnetzagentur.de/amateurfunk), Datenlizenz Deutschland – "
                   "Namensnennung – Version 2.0 (www.govdata.de/dl-de/by-2-0)");

struct ImportStats
{
    int questions = 0;
    int withQuestionImage = 0;
    int withAnswerImages = 0;
    int withLatex = 0;
    int pictureOnlyQuestions = 0;
    QStringList warnings;
    QSet<QString> imageNames;   ///< SVG base names that must be copied
};

bool containsLatex(const QString &text)
{
    return text.contains(QLatin1Char('$')) || text.contains(QLatin1String("\\("))
           || text.contains(QLatin1String("\\["));
}

QString slug(const QString &text)
{
    QString result = QString(text.toLower())
                          .replace(QRegularExpression(QStringLiteral("[^a-z0-9äöüß]+")),
                                   QStringLiteral("-"));
    result.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return result;
}

QString makeId(char letter, const QStringList &path)
{
    QString id = QStringLiteral("DE-%1-").arg(QLatin1Char(letter));
    QStringList clean;
    for (const QString &segment : path) {
        const QString part = slug(segment);
        if (!part.isEmpty())
            clean.append(part);
    }
    if (clean.isEmpty())
        clean.append(QStringLiteral("teil"));
    return id + clean.join(QLatin1Char('-'));
}

/// Reads one image reference such as "AB109_q" and records the SVG for copying.
QString resolveImage(const QJsonObject &question, const QString &key, ImportStats *stats)
{
    const QString name = question.value(key).toString().trimmed();
    if (name.isEmpty())
        return QString();
    if (name.isEmpty())
        return QString();
    stats->imageNames.insert(name);
    return QStringLiteral("images/%1.svg").arg(name);
}

/// Appends one source question to a chapter.
void fillChapter(Chapter *chapter, const QJsonObject &source, const QDate &addedOn,
                 ImportStats *stats)
{
    Question question;
    question.id = source.value(QStringLiteral("number")).toString().trimmed();
    question.order = chapter->questions.size() + 1;
    question.text = source.value(QStringLiteral("question")).toString().trimmed();
    question.points = 0; // not published by the source

    question.image = resolveImage(source, QStringLiteral("picture_question"), stats);
    if (!question.image.isEmpty())
        ++stats->withQuestionImage;

    bool hasLatex = containsLatex(question.text);
    static const char letters[] = {'a', 'b', 'c', 'd'};
    for (int i = 0; i < 4; ++i) {
        const QString letter = QString::fromLatin1(&letters[i], 1);
        Answer answer;
        answer.id = question.id + QLatin1Char('-') + letter;
        answer.order = i + 1;
        answer.text = source.value(QStringLiteral("answer_") + letter).toString().trimmed();
        answer.image = resolveImage(source, QStringLiteral("picture_") + letter, stats);
        if (!answer.image.isEmpty())
            ++stats->withAnswerImages;
        // The source catalog always lists the correct answer first.
        answer.correct = (i == 0);
        if (containsLatex(answer.text))
            hasLatex = true;
        question.answers.append(answer);
    }

    if (question.text.isEmpty() && question.image.isEmpty()) {
        stats->warnings << QStringLiteral("%1: weder Fragetext noch Fragebild").arg(question.id);
    }
    if (question.text.isEmpty() && !question.image.isEmpty())
        ++stats->pictureOnlyQuestions;

    const bool allTextless = std::none_of(question.answers.cbegin(), question.answers.cend(),
                                          [](const Answer &answer) { return !answer.text.isEmpty(); });
    if (allTextless)
        ++stats->pictureOnlyQuestions;

    question.textFormat = hasLatex ? TextFormat::Latex : TextFormat::Plain;
    if (hasLatex)
        ++stats->withLatex;

    question.history.addedOn = addedOn;
    chapter->questions.append(question);
    ++stats->questions;
}

/// Recursively gathers the questions of one source section that belong to the
/// selected class.
void collectQuestions(const QJsonArray &sections, const QString &classFilter,
                      QVector<QJsonObject> *sink)
{
    for (const QJsonValue &value : sections) {
        const QJsonObject section = value.toObject();
        const QJsonArray own = section.value(QStringLiteral("questions")).toArray();
        for (const QJsonValue &questionValue : own) {
            const QJsonObject question = questionValue.toObject();
            if (question.value(QStringLiteral("class")).toString() == classFilter)
                sink->append(question);
        }
        collectQuestions(section.value(QStringLiteral("sections")).toArray(), classFilter, sink);
    }
}

/// Builds one catalog from the source tree.
Catalog buildCatalog(const QJsonObject &root, const QJsonObject &metadata, ExamClass examClass,
                     char letter, const QString &classFilter, ImportStats *stats)
{
    Catalog catalog;

    const QString edition = metadata.value(QStringLiteral("edition")).toString();
    const QDate validFrom = QDate::fromString(
        metadata.value(QStringLiteral("valid_from")).toString(), Qt::ISODate);

    catalog.metadata.title = QStringLiteral("Prüfungsfragen Klasse %1 – %2")
                                 .arg(QLatin1Char(letter), edition);
    catalog.metadata.examClass = examClass;
    catalog.metadata.examId = QStringLiteral("DE-%1").arg(QLatin1Char(letter));
    catalog.metadata.authority = QStringLiteral(
        "Bundesnetzagentur für Elektrizität, Gas, Telekommunikation, Post und Eisenbahnen");
    catalog.metadata.catalogVersion = edition;
    catalog.metadata.validFrom = validFrom;
    catalog.metadata.language = QStringLiteral("de");
    catalog.metadata.sourceUrl = QStringLiteral("https://www.bundesnetzagentur.de/amateurfunk");
    catalog.metadata.license =
        metadata.value(QStringLiteral("license")).toString(QLatin1String("DL-DE-BY-2.0"));
    catalog.metadata.attribution = kAttribution;
    catalog.metadata.modifiedSinceSource = false;
    catalog.metadata.defaultPoints = 0;
    catalog.metadata.notes = QStringLiteral(
        "Fehlerpunkte werden in der Quelle nicht veröffentlicht und stehen daher auf 0. Nach der "
        "ersten Änderung von Hand modifiedSinceSource auf true setzen und den Quellenvermerk "
        "anpassen.");

    int partOrder = 0;
    for (const QJsonValue &topValue : root.value(QStringLiteral("sections")).toArray()) {
        const QJsonObject top = topValue.toObject();
        const QString topTitle = top.value(QStringLiteral("title")).toString();

        QVector<QJsonObject> topQuestions;
        collectQuestions(QJsonArray{top}, classFilter, &topQuestions);
        if (topQuestions.isEmpty())
            continue;

        ++partOrder;
        // Identifiers follow the position in the source tree, not the titles:
        // renaming a title must not invalidate the identity of a section.
        const QStringList partPath{QStringLiteral("teil%1").arg(partOrder)};

        Part part;
        part.title = topTitle;
        part.order = partOrder;
        part.id = makeId(letter, partPath);

        int sectionOrder = 0;
        const QJsonArray midSections = top.value(QStringLiteral("sections")).toArray();

        if (midSections.isEmpty()) {
            // Prüfungsteil holds questions directly: synthesise one section.
            ++sectionOrder;
            Section section;
            section.title = topTitle;
            section.order = sectionOrder;
            section.id = makeId(letter,
                                partPath + QStringList{QStringLiteral("abschnitt1")});
            Chapter chapter;
            chapter.title = topTitle;
            chapter.order = 1;
            chapter.id = makeId(letter,
                                partPath + QStringList{QStringLiteral("abschnitt1"),
                                                      QStringLiteral("kapitel1")});
            for (const QJsonObject &question : topQuestions)
                fillChapter(&chapter, question, validFrom, stats);
            section.chapters.append(chapter);
            part.sections.append(section);
        } else {
            for (const QJsonValue &midValue : midSections) {
                const QJsonObject mid = midValue.toObject();
                QVector<QJsonObject> midQuestions;
                collectQuestions(QJsonArray{mid}, classFilter, &midQuestions);
                if (midQuestions.isEmpty())
                    continue;

                ++sectionOrder;
                Section section;
                section.title = mid.value(QStringLiteral("title")).toString();
                section.order = sectionOrder;
                const QStringList sectionPath = partPath
                                                + QStringList{QStringLiteral("abschnitt%1")
                                                                  .arg(sectionOrder)};
                section.id = makeId(letter, sectionPath);

                int chapterOrder = 0;
                const QJsonArray leafSections = mid.value(QStringLiteral("sections")).toArray();
                const QJsonArray unitSections = leafSections.isEmpty() ? QJsonArray{mid} : leafSections;

                for (const QJsonValue &unitValue : unitSections) {
                    const QJsonObject unit = unitValue.toObject();
                    QVector<QJsonObject> unitQuestions;
                    collectQuestions(QJsonArray{unit}, classFilter, &unitQuestions);
                    if (unitQuestions.isEmpty())
                        continue;

                    ++chapterOrder;
                    Chapter chapter;
                    chapter.title = unit.value(QStringLiteral("title")).toString();
                    chapter.order = chapterOrder;
                    chapter.id = makeId(letter, sectionPath
                                                     + QStringList{QStringLiteral("kapitel%1")
                                                                   .arg(chapterOrder)});
                    for (const QJsonObject &question : unitQuestions)
                        fillChapter(&chapter, question, validFrom, stats);
                    section.chapters.append(chapter);
                }
                if (!section.chapters.isEmpty())
                    part.sections.append(section);
            }
        }

        if (!part.sections.isEmpty())
            catalog.parts.append(part);
    }

    return catalog;
}

int copyImages(const QDir &sourceSvgDir, const QDir &outputDir, const QSet<QString> &names,
               QTextStream &out)
{
    QDir target(outputDir.filePath(QStringLiteral("images")));
    if (!target.mkpath(QStringLiteral("."))) {
        out << QStringLiteral("Bilderverzeichnis nicht anlegbar: %1\n").arg(target.path());
        return 1;
    }

    int copied = 0;
    int missing = 0;
    for (const QString &name : std::as_const(names)) {
        const QString fileName = name + QStringLiteral(".svg");
        const QString source = sourceSvgDir.filePath(fileName);
        if (!QFileInfo::exists(source)) {
            ++missing;
            out << QStringLiteral("fehlt in der Quelle: %1\n").arg(fileName);
            continue;
        }
        const QString destination = target.filePath(fileName);
        // Re-imports must refresh images that the source may have changed.
        QFile::remove(destination);
        if (QFile::copy(source, destination))
            ++copied;
        else
            out << QStringLiteral("nicht kopierbar: %1\n").arg(fileName);
    }

    out << QStringLiteral("Bilder kopiert: %1, fehlend: %2\n").arg(copied).arg(missing);
    return missing > 0 ? 1 : 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hrt-catalog-import"));
    QCoreApplication::setApplicationVersion(QStringLiteral(HRT_VERSION_STRING));

    QTextStream out(stdout);
    QTextStream err(stderr);
    out.setEncoding(QStringConverter::Utf8);
    err.setEncoding(QStringConverter::Utf8);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Importiert den Fragenkatalog der Bundesnetzagentur in das "
                       "Klartext-XML-Format von HamRadioTrainer 2026."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption sourceOption(
        QStringLiteral("source"),
        QStringLiteral("Verzeichnis mit fragenkatalog3b.json und svgs/."),
        QStringLiteral("dir"));
    parser.addOption(sourceOption);
    QCommandLineOption outputOption(
        QStringLiteral("output"),
        QStringLiteral("Zielverzeichnis für die erzeugten Kataloge und Bilder."),
        QStringLiteral("dir"));
    parser.addOption(outputOption);
    parser.process(app);

    const QString sourceDirPath = parser.isSet(sourceOption) ? parser.value(sourceOption)
                                                             : parser.positionalArguments().value(0);
    const QString outputDirPath = parser.isSet(outputOption) ? parser.value(outputOption) : QString();

    if (sourceDirPath.isEmpty() || outputDirPath.isEmpty()) {
        err << QStringLiteral("Bitte --source und --output angeben. Siehe --help.\n");
        return 2;
    }

    const QDir sourceDir(sourceDirPath);
    QFile jsonFile(sourceDir.filePath(QStringLiteral("fragenkatalog3b.json")));
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        err << QStringLiteral("fragenkatalog3b.json nicht lesbar: %1\n").arg(jsonFile.errorString());
        return 2;
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        err << QStringLiteral("JSON-Fehler: %1\n").arg(parseError.errorString());
        return 2;
    }

    const QJsonObject root = document.object();
    const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
    const QDir outputDir(outputDirPath);
    if (!outputDir.mkpath(QStringLiteral("."))) {
        err << QStringLiteral("Ausgabeverzeichnis nicht anlegbar: %1\n").arg(outputDir.path());
        return 2;
    }

    struct Target
    {
        ExamClass examClass;
        char letter;
        QString filter;
    };
    const QVector<Target> targets = {{ExamClass::Novize, 'N', QStringLiteral("1")},
                                     {ExamClass::Einsteiger, 'E', QStringLiteral("2")},
                                     {ExamClass::Allgemein, 'A', QStringLiteral("3")}};

    QSet<QString> allImages;
    int failures = 0;

    for (const Target &target : targets) {
        ImportStats stats;
        const Catalog catalog =
            buildCatalog(root, metadata, target.examClass, target.letter, target.filter, &stats);
        if (catalog.parts.isEmpty()) {
            err << QStringLiteral("Klasse %1: keine Fragen gefunden\n").arg(QLatin1Char(target.letter));
            ++failures;
            continue;
        }

        const QString fileName = QStringLiteral("de-amateurfunk-%1.xml").arg(QLatin1Char(target.letter));
        const QString path = outputDir.filePath(fileName);
        QString error;
        if (!CatalogXml::save(path, catalog, &error)) {
            err << QStringLiteral("%1: %2\n").arg(fileName, error);
            ++failures;
            continue;
        }

        QVector<CatalogIssue> issues;
        const bool usable = CatalogValidator::validate(catalog, &issues, outputDirPath);
        for (const CatalogIssue &issue : issues) {
            (issue.severity == IssueSeverity::Error ? err : out)
                << fileName << QLatin1String(": ")
                << (issue.severity == IssueSeverity::Error ? QStringLiteral("FEHLER ")
                                                           : QStringLiteral("Hinweis "))
                << issue.location << QLatin1String(" — ") << issue.message << Qt::endl;
        }
        if (!usable)
            ++failures;

        allImages.unite(stats.imageNames);

        out << QStringLiteral("%1: %2 Fragen, %3 mit Fragebild, %4 mit Antwortbildern, "
                              "%5 mit LaTeX, %6 reine Bildfragen\n")
                    .arg(fileName)
                    .arg(stats.questions)
                    .arg(stats.withQuestionImage)
                    .arg(stats.withAnswerImages)
                    .arg(stats.withLatex)
                    .arg(stats.pictureOnlyQuestions);
    }

    failures += copyImages(sourceDir.filePath(QStringLiteral("svgs")), outputDir, allImages, out);

    return failures > 0 ? 1 : 0;
}