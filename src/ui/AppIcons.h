// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

#include <QIcon>
#include <QStringList>

namespace hrt {

/// The icons of the program.
///
/// They ship with the program instead of being taken from the desktop theme,
/// because a learner on Windows has no icon theme at all and a learner on Linux
/// would see a different set of pictures from one day to the next. Own icons
/// look the same everywhere.
///
/// Every icon exists twice, once with dark ink and once with light ink. A
/// single grey set disappears into either a light or a dark background, and Qt
/// cannot recolour an SVG from outside - hence the second file.
///
/// A name that has no file yields a null icon. That is deliberate: a missing
/// icon leaves the button readable, while a wrong one misleads.
class AppIcons
{
public:
    /// One icon, chosen for the colour scheme in effect. The result is cached.
    static QIcon get(const QString &name);

    /// Every icon name that ships with the program, unsorted. For tests, which
    /// have to notice an icon nobody uses any more.
    static QStringList names();

private:
    static QString fileFor(const QString &name);
};

} // namespace hrt
