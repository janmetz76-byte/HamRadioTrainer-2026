// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "core/LatexFormatter.h"

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QXmlStreamReader>
#include <QtTest>

using namespace hrt;

class LatexFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void symbolsAreReplacedByTypicalFormulas_data();
    void symbolsAreReplacedByTypicalFormulas();

    void textOutsideTheFormulaStaysLiteral();
    void severalFormulasInOneText();
    void htmlIsEscaped();
    void unclosedDelimiterIsKept();
    void unknownCommandIsKeptAndReported();
    void formulaCountIsCounted();

    void everyFormulaOfTheCatalogsRenders();
    void noCommandSurvivesOutsideOfAFormula();
    void displayFormulaIsRecognised();
};

/// The exact constructs that occur in the 355 formula places of the catalogs.
void LatexFormatterTest::symbolsAreReplacedByTypicalFormulas_data()
{
    QTest::addColumn<QString>("latex");
    QTest::addColumn<QString>("expected");

    QTest::newRow("griechisch lambda") << QStringLiteral("\\lambda") << QStringLiteral("λ");
    QTest::newRow("griechisch Omega") << QStringLiteral("\\Omega") << QStringLiteral("Ω");
    QTest::newRow("griechisch eta") << QStringLiteral("\\eta") << QStringLiteral("η");
    QTest::newRow("griechisch varphi") << QStringLiteral("\\varphi") << QStringLiteral("φ");

    QTest::newRow("punkt") << QStringLiteral("5 \\cdot 10^{-1}")
                           << QStringLiteral("5 · 10<sup>-1</sup>");
    QTest::newRow("hochgestellt mit Minus") << QStringLiteral("2\\cdot 10^{-7}")
                                            << QStringLiteral("2· 10<sup>-7</sup>");
    QTest::newRow("tiefgestellt mit Text") << QStringLiteral("R_{\\textrm{L}} = 8,2 k\\Omega")
                                            << QStringLiteral("R<sub>L</sub> = 8,2 kΩ");
    QTest::newRow("verschachteltes dfrac")
        << QStringLiteral("\\left(\\eta = \\dfrac{P_{\\textrm{L}}}{P_{\\textrm{IN}}}\\right)")
        << QStringLiteral("(η = <sup>P<sub>L</sub></sup>⁄<sub>P<sub>IN</sub></sub>)");
    QTest::newRow("frac mit Oben und Nenner")
        << QStringLiteral("\\frac{E}{{E}_{\\textrm{max}}}")
        << QStringLiteral("<sup>E</sup>⁄<sub>E<sub>max</sub></sub>");
    QTest::newRow("hochgestellter Grad") << QStringLiteral("x^{\\circ}")
                                          << QStringLiteral("x<sup>°</sup>");
    QTest::newRow("text") << QStringLiteral("a_{\\text{BW}}") << QStringLiteral("a<sub>BW</sub>");
    QTest::newRow("geschuetztes Leerzeichen") << QStringLiteral("a~b")
                                              << QStringLiteral("a&nbsp;b");
    QTest::newRow("leerzeichenbefehle") << QStringLiteral("5\\,m\\;n")
                                        << QStringLiteral("5 m n");
}

void LatexFormatterTest::symbolsAreReplacedByTypicalFormulas()
{
    QFETCH(QString, latex);
    QFETCH(QString, expected);

    QCOMPARE(LatexFormatter::formulaToRichText(latex), expected);
    QVERIFY2(LatexFormatter::isComplete(QStringLiteral("$%1$").arg(latex)), qPrintable(latex));
}

void LatexFormatterTest::textOutsideTheFormulaStaysLiteral()
{
    QCOMPARE(LatexFormatter::toRichText(QStringLiteral("Die Wellenlänge ist $\\lambda$ lang.")),
             QStringLiteral("Die Wellenlänge ist λ lang."));
    QCOMPARE(LatexFormatter::toRichText(QStringLiteral("Ohne Formel")), QStringLiteral("Ohne Formel"));
    QVERIFY(LatexFormatter::toRichText(QString()).isEmpty());
}

void LatexFormatterTest::severalFormulasInOneText()
{
    QCOMPARE(LatexFormatter::toRichText(QStringLiteral("$a$ und $b$ und $c$")),
             QStringLiteral("a und b und c"));
}

void LatexFormatterTest::htmlIsEscaped()
{
    // Text outside a formula must not be able to inject markup.
    QCOMPARE(LatexFormatter::toRichText(QStringLiteral("<b>fett</b> & mehr")),
             QStringLiteral("&lt;b&gt;fett&lt;/b&gt; &amp; mehr"));
}

void LatexFormatterTest::unclosedDelimiterIsKept()
{
    const QString text = QStringLiteral("Wert $x ist groß");
    QVERIFY(!LatexFormatter::isComplete(text));
    QVERIFY(LatexFormatter::toRichText(text).contains(QStringLiteral("x ist groß")));
}

void LatexFormatterTest::unknownCommandIsKeptAndReported()
{
    // A command that is not covered must stay visible instead of vanishing.
    const QString text = QStringLiteral("$\\int_0^1 x$");
    QVERIFY(!LatexFormatter::isComplete(text));
    const QString rendered = LatexFormatter::toRichText(text);
    QVERIFY2(rendered.contains(QStringLiteral("\\int")), "unbekannter Befehl muss sichtbar bleiben");
    QVERIFY(rendered.contains(QStringLiteral("<sub>0</sub>")));
    QVERIFY(rendered.contains(QStringLiteral("<sup>1</sup>")));
}

void LatexFormatterTest::formulaCountIsCounted()
{
    QCOMPARE(LatexFormatter::formulaCount(QStringLiteral("$\\lambda$ und $f$")), 2);
    QCOMPARE(LatexFormatter::formulaCount(QStringLiteral("ohne")), 0);
    QCOMPARE(LatexFormatter::formulaCount(QStringLiteral("$offen")), 0);
}

/// The real check: every formula of the shipped catalogs has to render without
/// a leftover backslash. A gap would show up as "\\lambda" inside a question.
void LatexFormatterTest::everyFormulaOfTheCatalogsRenders()
{
    const QDir catalogDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"));

    int formulas = 0;
    QStringList problems;
    for (const QString &file :
         {QStringLiteral("de-amateurfunk-N.xml"), QStringLiteral("de-amateurfunk-E.xml"),
          QStringLiteral("de-amateurfunk-A.xml")}) {
        QFile handle(catalogDir.filePath(file));
        QVERIFY2(handle.open(QIODevice::ReadOnly), qPrintable(file));

        QXmlStreamReader reader(&handle);
        while (!reader.atEnd()) {
            reader.readNext();
            if (!reader.isStartElement())
                continue;
            if (reader.name() != QLatin1String("text") && reader.name() != QLatin1String("answer"))
                continue;

            const QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
            const int count = LatexFormatter::formulaCount(text);
            formulas += count;
            if (count == 0)
                continue;

            const QString rendered = LatexFormatter::toRichText(text);
            if (!LatexFormatter::isComplete(text) || rendered.contains(QLatin1Char('\\')))
                problems.append(QStringLiteral("%1: %2").arg(file, text.left(70)));
        }
    }

    QVERIFY2(formulas > 300, qPrintable(QStringLiteral("nur %1 Formeln gefunden").arg(formulas)));
    QVERIFY2(problems.isEmpty(),
             qPrintable(QStringLiteral("%1 von %2 Formen unvollständig: %3")
                            .arg(problems.size())
                            .arg(formulas)
                            .arg(problems.mid(0, 3).join(QStringLiteral(" | ")))));
}

/// The importer repairs the three places where the source writes markup
/// outside a formula. This guards the result: a human editing the XML must not
/// meet a stray backslash.
void LatexFormatterTest::noCommandSurvivesOutsideOfAFormula()
{
    const QDir catalogDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"));
    QFile handle(catalogDir.filePath(QStringLiteral("de-amateurfunk-A.xml")));
    QVERIFY(handle.open(QIODevice::ReadOnly));

    QXmlStreamReader reader(&handle);
    int checked = 0;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        if (reader.name() != QLatin1String("text") && reader.name() != QLatin1String("answer"))
            continue;
        const QString text = reader.readElementText(QXmlStreamReader::IncludeChildElements);
        ++checked;
        // Everything outside a formula may not contain a command any more.
        QString literal;
        int position = 0;
        while (position < text.size()) {
            int next = text.size();
            for (const QString &delimiter :
                 {QStringLiteral("$"), QStringLiteral("\\[")}) {
                const int found = text.indexOf(delimiter, position);
                if (found >= 0)
                    next = qMin(next, found);
            }
            literal += text.mid(position, next - position);
            if (next >= text.size())
                break;
            const bool inlineFormula = text.at(next) == QLatin1Char('$')
                                       && text.mid(next, 2) != QLatin1String("$$");
            const int bodyStart = next + (inlineFormula ? 1 : 2);
            const int bodyEnd = text.indexOf(inlineFormula ? QLatin1Char('$') : QLatin1Char(']'),
                                             bodyStart);
            if (bodyEnd < 0)
                break;
            position = bodyEnd + 1;
        }
        QVERIFY2(!literal.contains(QLatin1Char('\\')),
                 qPrintable(QStringLiteral("Befehl im Klartext: %1").arg(literal.left(60))));
    }
    QVERIFY(checked > 3000);
}

void LatexFormatterTest::displayFormulaIsRecognised()
{
    // The source uses "\\[…\\]" for one formula of class A.
    const QString text = QStringLiteral("Formel: \\[d = \\frac{\\sqrt{30 Ohm}}\\]");
    const QString rendered = LatexFormatter::toRichText(text);
    QVERIFY(!rendered.contains(QLatin1Char('\\')));
    QVERIFY(LatexFormatter::isComplete(text));
    QVERIFY(rendered.contains(QStringLiteral("⁄")));
}

QTEST_MAIN(LatexFormatterTest)
#include "tst_latex.moc"