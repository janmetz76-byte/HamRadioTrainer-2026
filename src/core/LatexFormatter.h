// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include <QString>

namespace hrt {

/// Renders the math markup of the BNetzA question catalogs.
///
/// The source keeps 219 of the 1750 questions of class A in LaTeX. The whole
/// vocabulary of these questions is small and was measured in the catalogs
/// themselves: 355 formula places, Greek letters, \cdot and \circ, a handful of
/// fractions, one \left…\right pair and no environment at all. So instead of
/// pulling in a complete TeX distribution, this formatter covers exactly what
/// occurs and stays readable.
///
/// A command that is not covered is kept verbatim instead of being dropped. A
/// formula that cannot be rendered must still show something, and the leftover
/// backslash makes the gap visible rather than hiding it.
class LatexFormatter
{
public:
    /// Renders a whole question or answer text. Everything outside $...$ is
    /// escaped and stays literal, everything inside becomes rich text.
    static QString toRichText(const QString &text);

    /// Renders one formula without the surrounding dollar signs.
    static QString formulaToRichText(const QString &latex);

    /// True when every formula of the text could be rendered completely.
    static bool isComplete(const QString &text);

    /// Number of formula places in the text.
    static int formulaCount(const QString &text);
};

} // namespace hrt