// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "CatalogLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace hrt {
namespace {

/// The catalogs of a class, in the order they are offered.
const QStringList kCatalogFiles = {
    QStringLiteral("de-amateurfunk-N.xml"),
    QStringLiteral("de-amateurfunk-E.xml"),
    QStringLiteral("de-amateurfunk-A.xml"),
};

} // namespace

QStringList CatalogLocator::searchDirectories()
{
    QStringList directories;

    // 1. Next to the program. This covers a portable installation and the
    //    AppImage, where the catalogs are unpacked next to the binary.
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    if (!applicationDirectory.isEmpty()) {
        directories << applicationDirectory
                    << QDir(applicationDirectory).filePath(QStringLiteral("data/catalogs"));
#ifdef Q_OS_MACOS
        // Inside an application bundle the catalogs live next to the bundle.
        directories << QDir(applicationDirectory).filePath(QStringLiteral("../Resources/catalogs"));
#endif
    }

    // 2. Where the packaging installs them.
    //
    // The prefix is baked in at configure time, but it is not the same for
    // every way of installing: a distribution package goes to /usr, a plain
    // "make install" goes to /usr/local. Both are therefore checked, along
    // with whatever this build was configured for.
    const QString relative = QStringLiteral(HRT_CATALOG_RELATIVE_DIR);
    const QStringList prefixes = {QStringLiteral(HRT_INSTALL_DATA_DIR),
                                  QStringLiteral("/usr"),
                                  QStringLiteral("/usr/local")};
    for (const QString &prefix : prefixes) {
        const QString directory = QDir(prefix).filePath(relative);
        directories << QDir(directory).filePath(QStringLiteral("catalogs")) << directory;
    }

    // 3. A plain checkout, so the program runs before it was ever installed.
    directories << QDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs")).absolutePath();

    directories.removeDuplicates();
    return directories;
}

CatalogLocator::Result CatalogLocator::locate()
{
    Result result;
    const QStringList directories = searchDirectories();

    for (const QString &directory : directories) {
        if (!QFileInfo(directory).isDir())
            continue;

        QStringList found;
        for (const QString &file : kCatalogFiles) {
            const QString path = QDir(directory).filePath(file);
            if (QFileInfo::exists(path))
                found.append(path);
        }

        if (!found.isEmpty()) {
            result.paths = found;
            result.baseDirectory = QDir(directory).absolutePath();
            return result;
        }
    }

    result.problem = QCoreApplication::translate(
        "CatalogLocator", "Keine Fragenkataloge gefunden. Gesucht wurde in: %1")
                          .arg(directories.join(QStringLiteral(", ")));
    return result;
}

} // namespace hrt