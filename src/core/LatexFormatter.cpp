// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "LatexFormatter.h"

#include <QHash>
#include <QVector>
#include <QStringView>

namespace hrt {
namespace {

/// Unicode replacement of the single character commands that occur in the
/// catalogs. Everything here was found in the shipped questions.
const QHash<QString, QString> &symbolTable()
{
    static const QHash<QString, QString> table = {
        // Greek letters
        {QStringLiteral("lambda"), QStringLiteral("λ")},
        {QStringLiteral("mu"), QStringLiteral("μ")},
        {QStringLiteral("pi"), QStringLiteral("π")},
        {QStringLiteral("eta"), QStringLiteral("η")},
        {QStringLiteral("rho"), QStringLiteral("ρ")},
        {QStringLiteral("sigma"), QStringLiteral("σ")},
        {QStringLiteral("tau"), QStringLiteral("τ")},
        {QStringLiteral("phi"), QStringLiteral("φ")},
        {QStringLiteral("varphi"), QStringLiteral("φ")},
        {QStringLiteral("theta"), QStringLiteral("θ")},
        {QStringLiteral("omega"), QStringLiteral("ω")},
        {QStringLiteral("alpha"), QStringLiteral("α")},
        {QStringLiteral("beta"), QStringLiteral("β")},
        {QStringLiteral("gamma"), QStringLiteral("γ")},
        {QStringLiteral("delta"), QStringLiteral("δ")},
        {QStringLiteral("Delta"), QStringLiteral("Δ")},
        {QStringLiteral("Omega"), QStringLiteral("Ω")},
        {QStringLiteral("Sigma"), QStringLiteral("Σ")},
        {QStringLiteral("Phi"), QStringLiteral("Φ")},

        // Operators and relations
        {QStringLiteral("cdot"), QStringLiteral("·")},
        {QStringLiteral("circ"), QStringLiteral("°")},
        {QStringLiteral("times"), QStringLiteral("×")},
        {QStringLiteral("pm"), QStringLiteral("±")},
        {QStringLiteral("mp"), QStringLiteral("∓")},
        {QStringLiteral("approx"), QStringLiteral("≈")},
        {QStringLiteral("neq"), QStringLiteral("≠")},
        {QStringLiteral("leq"), QStringLiteral("≤")},
        {QStringLiteral("geq"), QStringLiteral("≥")},
        {QStringLiteral("cdotp"), QStringLiteral("·")},

        // Spacing
        {QStringLiteral(","), QStringLiteral(" ")},
        {QStringLiteral(";"), QStringLiteral(" ")},
        {QStringLiteral("!"), QString()},

        // Text
        {QStringLiteral("ldots"), QStringLiteral("…")},
        {QStringLiteral("cdots"), QStringLiteral("…")},
        {QStringLiteral("%"), QStringLiteral("%")},
        {QStringLiteral("&"), QStringLiteral("&")},
        {QStringLiteral("#"), QStringLiteral("#")},
        {QStringLiteral("_"), QStringLiteral("_")},
        {QStringLiteral("$"), QStringLiteral("$")},

        // Delimiters that only fix the size of what follows
        {QStringLiteral("left"), QString()},
        {QStringLiteral("right"), QString()},
        {QStringLiteral("displaystyle"), QString()},
        {QStringLiteral("textstyle"), QString()},
        {QStringLiteral("{"), QStringLiteral("{")},
        {QStringLiteral("}"), QStringLiteral("}")},
        {QStringLiteral("|"), QStringLiteral("|")},
    };
    return table;
}

/// Commands whose argument is written as text and stays upright.
bool isTextCommand(const QString &name)
{
    return name == QLatin1String("text") || name == QLatin1String("textrm")
           || name == QLatin1String("textbf") || name == QLatin1String("mathrm")
           || name == QLatin1String("mathsf") || name == QLatin1String("mbox");
}

bool isFractionCommand(const QString &name)
{
    return name == QLatin1String("frac") || name == QLatin1String("dfrac")
           || name == QLatin1String("tfrac");
}

/// Recursive descent over one formula fragment.
class FragmentParser
{
public:
    explicit FragmentParser(const QString &latex)
        : m_source(latex)
    {
    }

    QString parse()
    {
        return parseSequence(false);
    }

    /// Parses exactly one command at the beginning of the source. The source
    /// has to start with a backslash. Used for the stray commands that the
    /// source writes outside of any formula.
    QString parseLeadingCommand()
    {
        m_position = 0;
        if (atEnd() || peek() != QLatin1Char('\\'))
            return {};
        return parseCommand();
    }

    bool complete() const { return m_complete; }
    /// Number of characters the parser consumed.
    int consumed() const { return m_position; }

private:
    QString m_source;
    int m_position = 0;
    bool m_complete = true;

    bool atEnd() const { return m_position >= m_source.size(); }
    QChar peek() const { return m_source.at(m_position); }

    /// Parses until "}" when stopAtBrace is true, otherwise to the end.
    QString parseSequence(bool stopAtBrace)
    {
        QString out;
        while (!atEnd()) {
            const QChar character = peek();
            if (stopAtBrace && character == QLatin1Char('}'))
                break;
            if (character == QLatin1Char('\\'))
                out += parseCommand();
            else if (character == QLatin1Char('{')) {
                ++m_position;
                out += parseSequence(true);
                if (!atEnd())
                    ++m_position; // the closing brace
            } else if (character == QLatin1Char('_') || character == QLatin1Char('^')) {
                ++m_position;
                const QString argument = parseArgument();
                out += (character == QLatin1Char('_')) ? QStringLiteral("<sub>%1</sub>").arg(argument)
                                                        : QStringLiteral("<sup>%1</sup>").arg(argument);
            } else if (character == QLatin1Char('~')) {
                ++m_position;
                out += QStringLiteral("&nbsp;");
            } else {
                ++m_position;
                out += QString(character).toHtmlEscaped();
            }
        }
        return out;
    }

    /// The argument of _ or ^: a braced group or a single atom.
    QString parseArgument()
    {
        if (atEnd())
            return {};
        if (peek() == QLatin1Char('{')) {
            ++m_position;
            const QString inner = parseSequence(true);
            if (!atEnd())
                ++m_position;
            return inner;
        }
        if (peek() == QLatin1Char('\\'))
            return parseCommand();
        const QChar character = peek();
        ++m_position;
        return QString(character).toHtmlEscaped();
    }

    QString parseCommand()
    {
        ++m_position; // the backslash
        if (atEnd())
            return QStringLiteral("\\");

        // \{ \} \\ \% and similar single character commands
        const QChar character = peek();
        if (!character.isLetter()) {
            ++m_position;
            // "\left" and friends map to an empty string on purpose, so the
            // lookup has to test for the key and not for a non empty value.
            if (symbolTable().contains(QString(character)))
                return symbolTable().value(QString(character));
            return QString(character).toHtmlEscaped();
        }

        QString name;
        while (!atEnd() && peek().isLetter())
            name += m_source.at(m_position++);

        if (isTextCommand(name)) {
            if (!atEnd() && peek() == QLatin1Char('{')) {
                ++m_position;
                const QString inner = parseSequence(true);
                if (!atEnd())
                    ++m_position;
                return inner;
            }
            m_complete = false;
            return QStringLiteral("\\%1").arg(name);
        }

        if (name == QLatin1String("sqrt")) {
            // An index would need a radical sign drawn around it, which plain
            // rich text cannot do, so only the square root is shown.
            if (!atEnd() && peek() == QLatin1Char('[')) {
                ++m_position;
                while (!atEnd() && peek() != QLatin1Char(']'))
                    ++m_position;
                if (!atEnd())
                    ++m_position;
            }
            return QStringLiteral("√(%1)").arg(parseArgument());
        }

        if (isFractionCommand(name)) {
            const QString numerator = parseArgument();
            const QString denominator = parseArgument();
            // A stacked fraction is shown with the fraction slash, the form the
            // question texts already use where they write fractions in words.
            return QStringLiteral("<sup>%1</sup>⁄<sub>%2</sub>").arg(numerator, denominator);
        }

        if (symbolTable().contains(name))
            return symbolTable().value(name);

        // Not covered: keep it, so the gap is visible instead of silent.
        m_complete = false;
        return QStringLiteral("\\%1").arg(name);
    }
};

/// Copies literal text, escaping markup but still resolving LaTeX commands.
///
/// The source uses a few macros outside the formula delimiters, for example
/// "4 \\mOhm". Showing the backslash would be worse than showing the unit, so
/// the same command table is applied here.
QString renderLiteral(const QString &text, bool *complete)
{
    QString out;
    for (int i = 0; i < text.size(); ++i) {
        const QChar character = text.at(i);
        if (character != QLatin1Char('\\')) {
            out += QString(character).toHtmlEscaped();
            continue;
        }
        FragmentParser parser(QStringView(text).mid(i).toString());
        out += parser.parseLeadingCommand();
        if (!parser.complete())
            *complete = false;
        // Always advance by at least the backslash itself.
        i += qMax(1, parser.consumed()) - 1;
    }
    return out;
}

/// One formula found in a text.
struct FormulaSpan
{
    int start = 0;      ///< first character of the opening delimiter
    int bodyStart = 0;  ///< first character of the formula itself
    int bodyEnd = 0;    ///< one past the last character of the formula
    int end = 0;        ///< one past the closing delimiter
    QString closing;
};

/// Walks a text and reports every formula in it.
///
/// The source uses three delimiter styles: "$…$", "$$…$$" and the display form
/// "\\[…\\]". Counting and rendering share this walk, so a formula can never
/// be rendered and at the same time escape the checks.
struct FormulaScan
{
    QVector<FormulaSpan> spans;
    /// Position of a delimiter that is never closed, -1 otherwise.
    int unclosedAt = -1;
};

FormulaScan scanFormulas(const QString &text)
{
    FormulaScan scan;
    int position = 0;

    while (position < text.size()) {
        // The next delimiter has to start before the next backslash, otherwise
        // a command in the literal text would win.
        int next = -1;
        FormulaSpan span;

        if (text.mid(position, 2) == QLatin1String("\\[")) {
            span.closing = QLatin1String("\\]");
        } else if (text.mid(position, 2) == QLatin1String("$$")) {
            span.closing = QLatin1String("$$");
        } else if (text.at(position) == QLatin1Char('$')) {
            span.closing = QLatin1String("$");
        }

        if (!span.closing.isEmpty()) {
            span.start = position;
            span.bodyStart = position + span.closing.size();
            const int bodyEnd = text.indexOf(span.closing, span.bodyStart);
            if (bodyEnd < 0) {
                scan.unclosedAt = span.start;
                break; // unclosed, the rest stays literal text
            }
            span.bodyEnd = bodyEnd;
            span.end = bodyEnd + span.closing.size();
            scan.spans.append(span);
            position = span.end;
            continue;
        }

        const int backslash = text.indexOf(QLatin1Char('\\'), position);
        const int dollar = text.indexOf(QLatin1Char('$'), position);
        next = (backslash < 0) ? dollar : (dollar < 0 ? backslash : qMin(backslash, dollar));
        if (next < 0)
            break;
        position = next;
    }

    return scan;
}

QString render(const QString &text, bool *complete)
{
    QString out;
    bool allComplete = true;
    int position = 0;

    const FormulaScan scan = scanFormulas(text);
    if (scan.unclosedAt >= 0)
        allComplete = false;

    for (const FormulaSpan &span : scan.spans) {
        if (span.start > position) {
            out += renderLiteral(QStringView(text).mid(position, span.start - position).toString(),
                                 &allComplete);
        }
        FragmentParser parser(text.mid(span.bodyStart, span.bodyEnd - span.bodyStart));
        out += parser.parse();
        allComplete = allComplete && parser.complete();
        position = span.end;
    }

    if (position < text.size()) {
        // Whatever follows the last formula stays literal, including the tail
        // of an unclosed delimiter.
        out += renderLiteral(QStringView(text).mid(position).toString(), &allComplete);
    }

    if (complete)
        *complete = allComplete;
    return out;
}

} // namespace

QString LatexFormatter::toRichText(const QString &text)
{
    return render(text, nullptr);
}

QString LatexFormatter::formulaToRichText(const QString &latex)
{
    FragmentParser parser(latex);
    return parser.parse();
}

bool LatexFormatter::isComplete(const QString &text)
{
    bool complete = true;
    render(text, &complete);
    return complete;
}

int LatexFormatter::formulaCount(const QString &text)
{
    return static_cast<int>(scanFormulas(text).spans.size());
}

} // namespace hrt