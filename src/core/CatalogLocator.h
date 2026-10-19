// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include <QString>
#include <QStringList>

namespace hrt {

/// Finds the question catalogs that belong to the installation.
///
/// A packaged program must not depend on a catalog the user has to look for
/// first. The catalogs are installed together with the program, so they are
/// looked for in the places the packaging actually puts them.
class CatalogLocator
{
public:
    /// Where the catalogs of the running program were found.
    struct Result
    {
        /// One entry per catalog of class N, E and A, in that order.
        QStringList paths;
        /// The directory the search started from, for the display.
        QString baseDirectory;
        /// Why nothing was found, empty when it worked.
        QString problem;
    };

    /// Searches for the installed catalogs.
    ///
    /// The order is deliberate:
    ///   1. the directory of the program itself, which is where a portable
    ///      installation and an AppImage keep them
    ///   2. the compile time install prefix, which is where a distribution
    ///      package keeps them
    ///   3. the directories below the source tree, so the program also runs
    ///      straight from a checkout
    static Result locate();

    /// The directories that are searched, in order. Exposed for the display and
    /// for the tests.
    static QStringList searchDirectories();
};

} // namespace hrt