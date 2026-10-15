// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "core/LatexFormatter.h"

#include <QLabel>
#include <QString>

namespace hrt {

/// Puts a question text into a label, rendering the math when it carries any.
///
/// A QLabel in AutoText mode treats a string that looks like markup as markup,
/// so plain questions are switched to PlainText explicitly. Otherwise a
/// question containing a less-than sign would be swallowed.
inline void setQuestionText(QLabel *label, const QString &text, TextFormat format)
{
    if (format == TextFormat::Latex) {
        label->setTextFormat(Qt::RichText);
        label->setText(LatexFormatter::toRichText(text));
        return;
    }
    label->setTextFormat(Qt::PlainText);
    label->setText(text);
}

} // namespace hrt