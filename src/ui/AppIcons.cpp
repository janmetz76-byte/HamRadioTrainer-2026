// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "AppIcons.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QResource>
#include <QStyleHints>

/// Haelt die Ressourcen fest.
///
/// Sie liegen in einer statischen Bibliothek, und der Linker nimmt aus einem
/// Archiv nur das hinein, wonach auch gefragt wird. Ohne diese Verknuepfung
/// bleibt die Datei mit den Icons draussen und alle QIcon sind leer - ohne
/// Fehlermeldung, weil die Datei schlicht nicht vorhanden ist.
///
/// Die Funktion liegt im globalen Namensraum, nicht in hrt: Q_INIT_RESOURCE
/// traegt den umgebenden Namensraum in den Namen ein und wuerde hier ein
/// Symbol suchen, das es nicht gibt.
extern int qInitResources_hrt();
static const int ressourceVerankert = [] {
    qInitResources_hrt();
    return 0;
}();

namespace hrt {

namespace {

/// Reads the names out of the resource tree rather than keeping a second list
/// next to it. A list here would be one more place to forget an update.
QStringList discoverNames()
{
    static QStringList cached;
    static bool searched = false;
    if (searched)
        return cached;
    searched = true;

    QDir einhang(QStringLiteral(":/icons"));
    if (!einhang.exists())
        return cached;
    const QStringList dateien =
        einhang.entryList({QStringLiteral("*.svg")}, QDir::Files, QDir::Name);
    for (const QString &datei : dateien) {
        QString name = QFileInfo(datei).completeBaseName();
        if (name.endsWith(QStringLiteral("-dark")))
            name.chop(5);
        if (!cached.contains(name))
            cached.append(name);
    }
    return cached;
}

} // namespace

QString AppIcons::fileFor(const QString &name)
{
    // Qt's colorScheme() has been there since 6.5, which is the oldest Qt this
    // program supports. An application that does not say what it follows
    // stays on the light variant.
    const bool dunkel =
        QGuiApplication::styleHints()
        && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;

    QString pfad = QStringLiteral(":/icons/%1%2.svg");
    QString endung = dunkel ? QStringLiteral("-dark") : QString();
    // Manche Bilder haben absichtlich nur eine Fassung - vor allem das
    // Fenstersymbol. Fehlt die dunkle Variante, wird nicht still ein leeres
    // Bild geliefert, sondern es bleibt das helle: eine Glühbirne ist auf
    // dunklem Grund kein Fehler, nur schwerer zu sehen.
    if (!QFile::exists(pfad.arg(name, endung)))
        endung.clear();
    return pfad.arg(name, endung);
}

QIcon AppIcons::get(const QString &name)
{
    static QHash<QString, QIcon> cache;
    const auto it = cache.constFind(name);
    if (it != cache.constEnd())
        return it.value();

    // addFile instead of the QIcon(QString): both work from a resource, but
    // addFile is explicit about looking in the resource system, which keeps
    // the icons working when the program is started from a build directory.
    QIcon icon;
    const QString datei = fileFor(name);
    if (QFile::exists(datei)) {
        icon.addFile(datei);
        // The pictures are drawn at 24 units. Naming the sizes keeps them
        // sharp on a high dpi screen instead of letting the style stretch a
        // single raster size.
        for (const int seite : {16, 22, 24, 32, 48, 64}) {
            icon.addFile(datei, QSize(seite, seite));
        }
    }
    cache.insert(name, icon);
    return icon;
}

QStringList AppIcons::names()
{
    return discoverNames();
}

} // namespace hrt
