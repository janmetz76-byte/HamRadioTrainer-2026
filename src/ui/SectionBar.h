// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include <QString>
#include <QWidget>

class QLabel;

namespace hrt {

/// A grey band that names the block underneath it.
///
/// The alternative is a label among labels. After a screen full of tables and
/// rows the eye needs something that says "a different kind of thing starts
/// here", and that is what the band does: it separates without a border, it
/// cannot be mistaken for a field value, and it stays grey in a dark window.
///
/// The wording comes from the program, not from the style: the band says what
/// the block is, never how it looks.
class SectionBar : public QWidget
{
    Q_OBJECT

public:
    explicit SectionBar(const QString &title, QWidget *parent = nullptr);

    /// Changes the wording afterwards. Used where the block names itself.
    void setTitle(const QString &title);

private:
    QLabel *m_title = nullptr;
};

} // namespace hrt
