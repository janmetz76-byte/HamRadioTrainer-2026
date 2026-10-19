// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// The import of the Bundesnetzagentur question catalog, usable from the
// library and not only from the command line: the application needs it for
// "Fragenkatalog aktualisieren…".
//
// The question text is not covered by the GPL. It is redistributed under
// DL-DE-BY-2.0, which requires the verbatim source notice; the importer writes
// that notice into the metadata of every generated catalog.
#include "core/CatalogImporter.h"

#include "core/Catalog.h"
#include "core/CatalogValidator.h"
#include "core/CatalogXml.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace hrt {
namespace {


/// Prüfungsumfang fixed by Amtsblatt-Verfügung 29/2024, valid from 24 June 2024:
/// every sheet has 25 questions, one point per correct answer, 19 of the 25
/// points are required to pass, and class A gets 60 instead of 45 minutes.
struct ExamRules
{
    static constexpr int Questions = 25;
    static constexpr int PassScore = 19;
    static constexpr int MinutesStandard = 45;
    static constexpr int MinutesClassA = 60;

    static int errorPoints() { return Questions - PassScore; }
};

/// Mandatory source notice, taken verbatim from the README of the source.
///
/// Names the work, the publisher and where it was published. The licence in
/// words stands on its own line further down: bundling it here would print it
/// twice in the help, once mitten im Satz.
const QString kAttribution =
    QStringLiteral("Prüfungsfragen zum Erwerb von Amateurfunkprüfungsbescheinigungen, "
                   "Bundesnetzagentur, 3. Auflage, März 2024 "
                   "(www.bundesnetzagentur.de/amateurfunk)");

/// The licence under that name, as the publisher writes it.
const QString kLicenseName = QStringLiteral("Datenlizenz Deutschland – Namensnennung – "
                                            "Version 2.0 (www.govdata.de/dl-de/by-2-0)");

/// Herausgeber mit Anschrift. Die Kurzform ohne Adresse genügt dem
/// Quellenvermerk nicht: wer den Datensatz weitergeben will, muss wissen, woher
/// er kommt.
const QString kAuthority = QStringLiteral(
    "Bundesnetzagentur für Elektrizität, Gas, Telekommunikation, Post und Eisenbahnen, "
    "Tulpenfeld 4, 53113 Bonn");

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
    stats->imageNames.insert(name);
    return QStringLiteral("images/%1.svg").arg(name);
}

/// Repairs markup that the source writes outside of any formula.
///
/// The catalogs are meant to be edited by hand, so they should not carry
/// LaTeX commands that no renderer would ever see. The source contains exactly
/// three such places: two siunitx unit macros and one German quotation mark.
QString tidySourceMarkup(const QString &text)
{
    static const QHash<QString, QString> replacements = {
        {QStringLiteral("\\mOhm"), QStringLiteral("mΩ")},
        {QStringLiteral("\\kiloOhm"), QStringLiteral("kΩ")},
        {QStringLiteral("\\Ohm"), QStringLiteral("Ω")},
        {QStringLiteral("\\glqq"), QStringLiteral("\u201e")},
        {QStringLiteral("\\grqq"), QStringLiteral("\u201c")},
    };

    QString result = text;
    for (auto it = replacements.cbegin(); it != replacements.cend(); ++it)
        result.replace(it.key(), it.value());
    return result;
}

/// Deterministic permutation of answer positions, derived from the question id.
///
/// Deliberately written without QRandomGenerator and qHash: a hash seeded per
/// process would produce a different catalog on every import, and a catalogue
/// that changes under our feet cannot be checked against anything. FNV-1a over
/// the id bytes followed by a Fisher-Yates shuffle is plain arithmetic and
/// repeats exactly, so tools/catalog-reshuffle-answers.py can reproduce it.
QVector<int> spreadAnswers(const QString &id, int count)
{
    quint32 hash = 2166136261u;
    const QByteArray bytes = id.toUtf8();
    for (const char byte : bytes) {
        hash ^= static_cast<quint32>(static_cast<unsigned char>(byte));
        hash *= 16777619u;
    }

    QVector<int> order(count);
    for (int i = 0; i < count; ++i)
        order[i] = i;
    for (int i = count - 1; i > 0; --i) {
        hash ^= hash << 13;
        hash ^= hash >> 17;
        hash ^= hash << 5;
        order.swapItemsAt(i, static_cast<int>(hash % static_cast<quint32>(i + 1)));
    }
    return order;
}

/// Appends one source question to a chapter.
void fillChapter(Chapter *chapter, const QJsonObject &source, const QDate &addedOn,
                 ImportStats *stats)
{
    Question question;
    question.id = source.value(QStringLiteral("number")).toString().trimmed();
    question.order = chapter->questions.size() + 1;
    question.text = tidySourceMarkup(source.value(QStringLiteral("question")).toString().trimmed());
    question.points = 0; // not published by the source

    question.image = resolveImage(source, QStringLiteral("picture_question"), stats);
    if (!question.image.isEmpty())
        ++stats->withQuestionImage;

    bool hasLatex = containsLatex(question.text);
    static const char letters[] = {'a', 'b', 'c', 'd'};
    QVector<Answer> sourceAnswers;
    for (int i = 0; i < 4; ++i) {
        const QString letter = QString::fromLatin1(&letters[i], 1);
        Answer answer;
        answer.id = question.id + QLatin1Char('-') + letter;
        answer.order = i + 1;
        answer.text = tidySourceMarkup(source.value(QStringLiteral("answer_") + letter).toString().trimmed());
        answer.image = resolveImage(source, QStringLiteral("picture_") + letter, stats);
        if (!answer.image.isEmpty())
            ++stats->withAnswerImages;
        // The source catalog always lists the correct answer first.
        answer.correct = (i == 0);
        if (containsLatex(answer.text))
            hasLatex = true;
        sourceAnswers.append(answer);
    }

    // The published catalog states in words that the right answer is always
    // A. We must not carry that habit over: a learner who sees it can pass
    // our simulation without knowing anything by ticking the top box. The
    // answers are therefore spread across the four positions, derived from
    // the question id so that repeated imports yield the same file.
    const QVector<int> order = spreadAnswers(question.id, sourceAnswers.size());
    for (int position = 0; position < order.size(); ++position) {
        Answer answer = sourceAnswers.at(order.at(position));
        answer.id = question.id + QLatin1Char('-') + QChar(QLatin1Char('a').unicode() + position);
        answer.order = position + 1;
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

/// Recursively gathers the questions of one source section. An empty
/// "classFilter" takes every question, which is what the two shared
/// Prüfungsteile need: see partIsClassSpecific().
void collectQuestions(const QJsonArray &sections, const QString &classFilter,
                      QVector<QJsonObject> *sink)
{
    for (const QJsonValue &value : sections) {
        const QJsonObject section = value.toObject();
        const QJsonArray own = section.value(QStringLiteral("questions")).toArray();
        for (const QJsonValue &questionValue : own) {
            const QJsonObject question = questionValue.toObject();
            if (classFilter.isEmpty()
                || question.value(QStringLiteral("class")).toString() == classFilter) {
                sink->append(question);
            }
        }
        collectQuestions(section.value(QStringLiteral("sections")).toArray(), classFilter, sink);
    }
}

/// The amtliche Prüfungsteile, as named in Amtsblatt-Verfügung 29/2024.
///
/// The exam has five sheets, not three: V and B are shared by all classes, and
/// the technical part exists once per class (N, E, A). A certificate of class N
/// covers V, B and N; class E covers V, B, N and E; class A covers all five.
/// A candidate who already holds a lower certificate only sits the sheets of
/// the higher classes, which is why the lower class sheets stay in the catalog.
struct ExamPart
{
    char letter;
    QString title;
    QString classFilter;  ///< empty means "every class"
};

const QVector<ExamPart> kExamParts = {
    {'V', QStringLiteral("Kenntnisse von Vorschriften"), QString()},
    {'B', QStringLiteral("Betriebliche Kenntnisse"), QString()},
    {'N', QStringLiteral("Technische Kenntnisse, Klasse N"), QStringLiteral("1")},
    {'E', QStringLiteral("Technische Kenntnisse, Klasse E"), QStringLiteral("2")},
    {'A', QStringLiteral("Technische Kenntnisse, Klasse A"), QStringLiteral("3")},
};

/// Technical sheets in increasing order of demand. A certificate covers all
/// of them up to and including the one of its own class, because a candidate
/// who already holds a lower certificate only sits the sheets above it.
const QStringList kTechnicalProgression = {QStringLiteral("N"), QStringLiteral("E"),
                                           QStringLiteral("A")};

/// Which of the five sheets a certificate of the given class consists of.
QVector<char> sheetsForClass(ExamClass examClass)
{
    const int technicalSheets = examClass == ExamClass::Novize      ? 1
                                : examClass == ExamClass::Einsteiger ? 2
                                                                     : 3;
    QVector<char> sheets{QChar('V').toLatin1(), QChar('B').toLatin1()};
    for (int i = 0; i < technicalSheets; ++i) {
        const QByteArray letter = kTechnicalProgression.at(i).toLatin1();
        sheets.append(letter.at(0));
    }
    return sheets;
}

/// Top level group of the source tree that holds the questions of a sheet.
QJsonArray sourceGroupFor(const QJsonObject &root, char letter, bool useSharedParts)
{
    const QJsonArray topSections = root.value(QStringLiteral("sections")).toArray();
    for (const QJsonValue &value : topSections) {
        const QString title = value.toObject().value(QStringLiteral("title")).toString();
        switch (letter) {
        case 'V':
            if (title.contains(QStringLiteral("Vorschriften")))
                return QJsonArray{value.toObject()};
            break;
        case 'B':
            if (title.contains(QStringLiteral("Betriebliche Kenntnisse")))
                return QJsonArray{value.toObject()};
            break;
        case 'N':
        case 'E':
        case 'A':
            if (title.contains(QStringLiteral("Technische Kenntnisse")))
                return QJsonArray{value.toObject()};
            break;
        default:
            break;
        }
    }
    Q_UNUSED(useSharedParts)
    return {};
}

/// Class filter for a sheet. V and B carry no class tag in the source at all,
/// so they are taken whole. The switch exists to reproduce the earlier, wrong
/// behaviour on demand for comparison.
QString filterFor(char letter, bool useSharedParts)
{
    if (letter == 'V' || letter == 'B')
        return useSharedParts ? QString() : QStringLiteral("1");
    switch (letter) {
    case 'N':
        return QStringLiteral("1");
    case 'E':
        return QStringLiteral("2");
    case 'A':
        return QStringLiteral("3");
    default:
        return QString();
    }
}

/// Builds one catalog from the source tree.
/// Builds the section/chapter tree of one Prüfungsteil from the source tree.
bool buildPartBody(const QJsonArray &group, const QString &classFilter, char letter,
                   int partIndex, Part *part, const QDate &addedOn, ImportStats *stats)
{
    const QStringList partPath{QStringLiteral("teil%1").arg(partIndex)};
    int sectionOrder = 0;
    bool any = false;

    const QJsonArray midSections = group.isEmpty()
                                       ? QJsonArray{}
                                       : group.first().toObject()
                                             .value(QStringLiteral("sections"))
                                             .toArray();

    // The Prüfungsteil holds questions directly: synthesise one section.
    if (midSections.isEmpty()) {
        ++sectionOrder;
        const QJsonObject own = group.first().toObject();
        QVector<QJsonObject> questions;
        collectQuestions(QJsonArray{own}, classFilter, &questions);
        if (questions.isEmpty())
            return false;

        Section section;
        section.title = own.value(QStringLiteral("title")).toString();
        section.order = sectionOrder;
        section.id = makeId(letter, partPath + QStringList{QStringLiteral("abschnitt1")});

        Chapter chapter;
        chapter.title = section.title;
        chapter.order = 1;
        chapter.id = makeId(letter,
                            partPath + QStringList{QStringLiteral("abschnitt1"),
                                                  QStringLiteral("kapitel1")});
        for (const QJsonObject &question : questions)
            fillChapter(&chapter, question, addedOn, stats);
        section.chapters.append(chapter);
        part->sections.append(section);
        return true;
    }

    for (const QJsonValue &midValue : midSections) {
        const QJsonObject mid = midValue.toObject();
        QVector<QJsonObject> midQuestions;
        collectQuestions(QJsonArray{mid}, classFilter, &midQuestions);
        if (midQuestions.isEmpty())
            continue;

        ++sectionOrder;
        const QStringList sectionPath =
            partPath + QStringList{QStringLiteral("abschnitt%1").arg(sectionOrder)};
        Section section;
        section.title = mid.value(QStringLiteral("title")).toString();
        section.order = sectionOrder;
        section.id = makeId(letter, sectionPath);

        const QJsonArray leafSections = mid.value(QStringLiteral("sections")).toArray();
        const QJsonArray unitSections =
            leafSections.isEmpty() ? QJsonArray{mid} : leafSections;

        int chapterOrder = 0;
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
            chapter.id = makeId(letter,
                                sectionPath + QStringList{QStringLiteral("kapitel%1")
                                                              .arg(chapterOrder)});
            for (const QJsonObject &question : unitQuestions)
                fillChapter(&chapter, question, addedOn, stats);
            section.chapters.append(chapter);
        }

        if (!section.chapters.isEmpty()) {
            part->sections.append(section);
            any = true;
        }
    }
    return any;
}

/// Builds one catalog: the five Prüfungsteile that a certificate of the given
/// class consists of.
Catalog buildCatalog(const QJsonObject &root, const QJsonObject &metadata, ExamClass examClass,
                     char letter, bool useSharedParts, ImportStats *stats)
{
    Catalog catalog;

    const QString edition = metadata.value(QStringLiteral("edition")).toString();
    const QDate validFrom =
        QDate::fromString(metadata.value(QStringLiteral("valid_from")).toString(), Qt::ISODate);

    // QString::arg(QChar, QString) gibt es erst ab Qt 6.12; der Import soll
    // aber schon mit 6.5 laufen. fromLatin1 ist die Form, die ueberall
    // funktioniert.
    catalog.metadata.title = QStringLiteral("Prüfungsfragen Klasse %1 – %2")
                                 .arg(QString::fromLatin1(&letter, 1), edition);
    catalog.metadata.examClass = examClass;
    catalog.metadata.examId = QStringLiteral("DE-%1").arg(QLatin1Char(letter));
    catalog.metadata.authority = kAuthority;
    catalog.metadata.catalogVersion = edition;
    catalog.metadata.validFrom = validFrom;
    catalog.metadata.language = QStringLiteral("de");
    catalog.metadata.sourceUrl = QStringLiteral("https://www.bundesnetzagentur.de/amateurfunk");
    // The source metadata carries the identifier with a stray arrow,
    // "DL-DE->BY-2.0". The published identifier on govdata.de is DL-DE-BY-2.0,
    // so the value is normalised here.
    catalog.metadata.license =
        metadata.value(QStringLiteral("license"))
            .toString(QLatin1String("DL-DE-BY-2.0"))
            .replace(QLatin1String("->"), QLatin1String("-"));
    catalog.metadata.licenseName = kLicenseName;
    catalog.metadata.attribution = kAttribution;
    // DL-DE-BY-2.0 verlangt, eine Aenderung am fremden Datensatz kenntlich zu
    // machen. Geraenderungen gibt es: die Lizenzkennung wurde oben von der
    // fehlerhaften Schreibweise der Quelle normalisiert. Das ist als Aenderung
    // zu kennzeichnen, sonst behauptet der Katalog unveraendert zu sein.
    catalog.metadata.modifiedSinceSource = true;

    // Amtsblatt-Verfügung 29/2024: one point per correct answer. The source does
    // not publish a value per question, so the value is taken from the
    // Prüfungsordnung and recorded here once for the whole catalog.
    catalog.metadata.defaultPoints = 1;
    catalog.metadata.notes = QStringLiteral(
        "Prüfungsumfang nach Amtsblatt-Verfügung 29/2024 (gültig ab 24.06.2024): jeder "
        "Prüfungsteil hat 25 Fragen, ein Punkt je richtiger Antwort, bestanden ab 19 Punkten, "
        "also 6 zulässige Fehler. Prüfungsteil A hat 60 Minuten, die übrigen 45. Jede Frage "
        "zählt damit einen Fehlerpunkt. "
        "Geändert gegenüber der Quelle der Bundesnetzagentur: die Lizenzkennung "
        "war dort fehlerhaft als 'DL-DE->BY-2.0' angegeben und wurde auf die "
        "veröffentlichte Kennung DL-DE-BY-2.0 berichtigt. Außerdem ist in der "
        "Quelle durchgehend die erste Antwort richtig; da die Prüfungsbögen die "
        "Antworten mischen, wurde die Position der richtigen Antwort hier "
        "gleichmäßig auf alle vier Plätze verteilt. Fragetext, Antworttexte und "
        "Bildverweise sind unverändert.");

    const QVector<char> sheets = sheetsForClass(examClass);
    int partIndex = 0;

    for (const ExamPart &examPart : kExamParts) {
        if (!sheets.contains(examPart.letter))
            continue;

        const QJsonArray group = sourceGroupFor(root, examPart.letter, useSharedParts);
        if (group.isEmpty())
            continue;

        ++partIndex;
        Part part;
        part.id = makeId(letter,
                         {QStringLiteral("teil%1").arg(partIndex),
                          QString::fromLatin1(&examPart.letter, 1).toLower()});
        // Der Titel nennt das Blatt und seinen Inhalt. Die Verfuegung, nach
        // der der Umfang bemisst ist, steht in den Notizen des Katalogs und
        // in der Hilfe - an jeder Zeile hier nur wiederholt, stoert sie beim
        // Lesen der Liste der Pruefungsteile.
        part.title = QStringLiteral("%1 (%2)")
                         .arg(QString::fromLatin1(&examPart.letter, 1), examPart.title);
        part.order = partIndex;
        part.selection.questions = ExamRules::Questions;
        part.selection.errorPoints = ExamRules::errorPoints();
        part.selection.passScore = ExamRules::PassScore;
        part.selection.minutes = examPart.letter == 'A' ? ExamRules::MinutesClassA
                                                        : ExamRules::MinutesStandard;

        if (!buildPartBody(group, filterFor(examPart.letter, useSharedParts), letter,
                           partIndex, &part, validFrom, stats)) {
            continue;
        }
        catalog.parts.append(part);
    }

    return catalog;
}

/// Kopiert genau die Bilder, die der neue Katalog braucht.
///
/// Fehlende Bilder sind ein Fehler: eine Bildfrage ohne Bild ist im Lernmodus
/// nicht beantwortbar.
struct ImageCopyResult
{
    int copied = 0;
    int missing = 0;
    QStringList errors;
};

ImageCopyResult copyImages(const QDir &sourceSvgDir, const QDir &outputDir,
                           const QSet<QString> &names)
{
    ImageCopyResult result;
    QDir target(outputDir.filePath(QStringLiteral("images")));
    if (!target.mkpath(QStringLiteral("."))) {
        result.errors.append(QObject::QObject::tr("Bilderverzeichnis nicht anlegbar: %1")
                                 .arg(target.path()));
        result.missing = 1;
        return result;
    }

    for (const QString &name : std::as_const(names)) {
        const QString fileName = name + QStringLiteral(".svg");
        const QString source = sourceSvgDir.filePath(fileName);
        if (!QFileInfo::exists(source)) {
            ++result.missing;
            result.errors.append(QObject::QObject::tr("fehlt in der Quelle: %1").arg(fileName));
            continue;
        }
        const QString destination = target.filePath(fileName);
        // Re-imports muessen Bilder erneuern, die sich in der Quelle geaendert
        // haben koennen.
        QFile::remove(destination);
        if (QFile::copy(source, destination))
            ++result.copied;
        else
            result.errors.append(QObject::QObject::tr("nicht kopierbar: %1").arg(fileName));
    }
    return result;
}

/// Loescht Bilder, die der neue Import nicht mehr braucht.
///
/// Ohne das bleiben nach einem Import, bei dem die Bundesnetzagentur eine
/// Frage zurueckgezogen hat, deren Bild liegen - unsichtbar, aber sie
/// wachsen mit jedem Update.
void removeStaleImages(const QDir &outputDir, const QSet<QString> &wanted)
{
    const QDir images(outputDir.filePath(QStringLiteral("images")));
    if (!images.exists())
        return;
    for (const QString &name : images.entryList({QStringLiteral("*.svg")}, QDir::Files)) {
        // wanted haelt die Namen ohne Endung, so wie sie der Import sammelt.
        if (wanted.contains(QFileInfo(name).completeBaseName()))
            continue;
        QFile::remove(images.filePath(name));
    }
}

} // namespace

/// Schreibt den Sollstand der erzeugten Kataloge.
///
/// Das Programm vergleicht damit beim Start, ob die Katalogdatei noch die
/// ausgelieferte ist - und ob sie von Hand geaendert wurde. Ohne diese Datei
/// kann es das nicht sagen, und nach jedem Import waere ein frisch
/// aktualisierter Katalog fuer es ein fremder.
static void writeManifest(const QDir &target,
                          const QVector<CatalogImportEntry> &entries)
{
    QJsonArray catalogs;
    for (const CatalogImportEntry &entry : entries) {
        QFile file(entry.path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray bytes = file.readAll();

        Catalog catalog;
        QString error;
        int questions = 0;
        int images = 0;
        QString version;
        QDate validFrom;
        if (CatalogXml::load(entry.path, &catalog, &error)) {
            questions = catalog.activeQuestionCount();
            for (const Question *question : catalog.questions()) {
                if (!question->image.isEmpty())
                    ++images;
                for (const Answer &answer : question->answers) {
                    if (!answer.image.isEmpty())
                        ++images;
                }
            }
            version = catalog.metadata.catalogVersion;
            validFrom = catalog.metadata.validFrom;
        }

        QJsonObject object;
        object.insert(QStringLiteral("examClass"),
                      examClassToString(catalog.metadata.examClass));
        object.insert(QStringLiteral("file"), entry.fileName);
        object.insert(QStringLiteral("sha256"),
                      QString::fromLatin1(QCryptographicHash::hash(bytes,
                                                                 QCryptographicHash::Sha256)
                                              .toHex()));
        object.insert(QStringLiteral("questions"), questions);
        object.insert(QStringLiteral("images"), images);
        object.insert(QStringLiteral("catalogVersion"), version);
        if (validFrom.isValid())
            object.insert(QStringLiteral("validFrom"), validFrom.toString(Qt::ISODate));
        catalogs.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schema"), 1);
    root.insert(QStringLiteral("comment"),
                QStringLiteral("Sollstand der ausgelieferten Kataloge. Wird vom Import "
                               "hrt-catalog-import erzeugt; das Programm vergleicht damit "
                               "die geladene Katalogdatei."));
    root.insert(QStringLiteral("source"),
                QStringLiteral("Prüfungsfragen zum Erwerb von Amateurfunkprüfungsbescheinigungen, "
                               "Bundesnetzagentur"));
    root.insert(QStringLiteral("catalogs"), catalogs);

    QFile file(target.filePath(QStringLiteral("catalog-manifest.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.write("\n");
}

bool CatalogImporter::sourceIsUsable(const QString &sourceDir, QString *error)
{
    const QDir dir(sourceDir);
    const QString json = dir.filePath(QStringLiteral("fragenkatalog3b.json"));
    if (!QFileInfo::exists(json)) {
        if (error) {
            *error = QObject::tr("In dem Verzeichnis liegt keine fragenkatalog3b.json. "
                                 "Bitte das Archiv der Bundesnetzagentur entpacken und "
                                 "das Verzeichnis wählen, das die Datei enthält.");
        }
        return false;
    }
    QFile file(json);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("fragenkatalog3b.json nicht lesbar: %1")
                         .arg(file.errorString());
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()
        || document.object().value(QStringLiteral("metadata")).toObject().isEmpty()) {
        if (error) {
            *error = QObject::tr("fragenkatalog3b.json wurde gelesen, hat aber keinen "
                                 "Teil \"metadata\". Sieht das nach dem richtigen "
                                 "Archiv aus?");
        }
        return false;
    }
    return true;
}

CatalogImportReport CatalogImporter::importSource(const QString &sourceDir,
                                                  const QString &outputDir,
                                                  const CatalogImportOptions &options)
{
    CatalogImportReport report;

    QString reason;
    if (!sourceIsUsable(sourceDir, &reason)) {
        report.errors.append(reason);
        return report;
    }

    const QDir source(sourceDir);
    QFile jsonFile(source.filePath(QStringLiteral("fragenkatalog3b.json")));
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        report.errors.append(QObject::tr("fragenkatalog3b.json nicht lesbar: %1")
                                 .arg(jsonFile.errorString()));
        return report;
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        report.errors.append(QObject::tr("JSON-Fehler: %1").arg(parseError.errorString()));
        return report;
    }

    const QJsonObject root = document.object();
    const QJsonObject metadata = root.value(QStringLiteral("metadata")).toObject();
    const QDir target(outputDir);
    if (!target.mkpath(QStringLiteral("."))) {
        report.errors.append(QObject::tr("Ausgabeverzeichnis nicht anlegbar: %1")
                                 .arg(target.path()));
        return report;
    }

    struct Class
    {
        ExamClass examClass;
        char letter;
    };
    const QVector<Class> classes = {{ExamClass::Novize, 'N'},
                                    {ExamClass::Einsteiger, 'E'},
                                    {ExamClass::Allgemein, 'A'}};

    QSet<QString> allImages;
    QVector<Catalog> writtenCatalogs;
    for (const Class &entry : classes) {
        ImportStats stats;
        const Catalog catalog =
            buildCatalog(root, metadata, entry.examClass, entry.letter, options.sharedParts,
                         &stats);
        if (catalog.parts.isEmpty()) {
            report.errors.append(QObject::tr("Klasse %1: keine Fragen gefunden")
                                     .arg(QLatin1Char(entry.letter)));
            continue;
        }

        const QString fileName =
            QStringLiteral("de-amateurfunk-%1.xml").arg(QLatin1Char(entry.letter));
        const QString path = target.filePath(fileName);

        // Der alte Inhalt bleibt im Bericht, damit der Aufrufer entscheiden
        // kann, ob er eine Sicherung schreibt.
        QByteArray previous;
        QFile oldFile(path);
        if (oldFile.open(QIODevice::ReadOnly))
            previous = oldFile.readAll();

        QString error;
        if (!CatalogXml::save(path, catalog, &error)) {
            report.errors.append(QObject::tr("%1: %2").arg(fileName, error));
            continue;
        }

        allImages.unite(stats.imageNames);
        writtenCatalogs.append(catalog);
        CatalogImportEntry written;
        written.fileName = fileName;
        written.path = path;
        written.previousContent = previous;
        written.questions = stats.questions;
        report.catalogs.append(written);
    }

    const ImageCopyResult images =
        copyImages(source.filePath(QStringLiteral("svgs")), target, allImages);
    report.imagesCopied = images.copied;
    report.imagesMissing = images.missing;
    report.errors.append(images.errors);

    // Erst nach dem Kopieren aufräumen: bis dahin gehören die alten Bilder
    // noch zu den Katalogen, die gerade geschrieben wurden.
    if (report.errors.isEmpty())
        removeStaleImages(target, allImages);

    // Der Sollstand wird bei jedem Import neu geschrieben. Ein Katalog, den
    // jemand von Hand ergänzt hat, kommt darin nicht vor - für ihn bleibt
    // der zuletzt importierte Stand die Vergleichsbasis, und genau das
    // meldet der Prüfbericht als "von Hand geändert".
    if (!report.catalogs.isEmpty())
        writeManifest(target, report.catalogs);

    // Geprueft wird nach dem Kopieren der Bilder. Vorher gemeldete der Import
    // jede einzelne Bildfrage als "Bild nicht gefunden" - mehrere hundert
    // Scheinmeldungen, weil die Bilder noch gar nicht da waren.
    for (int i = 0; i < writtenCatalogs.size() && i < report.catalogs.size(); ++i) {
        const CatalogImportEntry &entry = report.catalogs.at(i);
        QVector<CatalogIssue> issues;
        const bool usable =
            CatalogValidator::validate(writtenCatalogs.at(i), &issues, target.path());
        for (const CatalogIssue &issue : issues) {
            if (issue.severity == IssueSeverity::Error) {
                report.errors.append(QObject::tr("%1: %2 — %3")
                                         .arg(entry.fileName, issue.location, issue.message));
            } else {
                report.warnings.append(QObject::tr("%1: %2 — %3")
                                           .arg(entry.fileName, issue.location, issue.message));
            }
        }
        if (!usable) {
            report.errors.append(QObject::tr("%1: nicht verwendbar, siehe die Fehler oben")
                                     .arg(entry.fileName));
        }
    }

    report.ok = report.errors.isEmpty();
    return report;
}

} // namespace hrt
