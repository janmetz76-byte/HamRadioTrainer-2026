// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "core/AppIdentity.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    hrt::setApplicationIdentity();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("HamRadioTrainer 2026 — Lern- und Prüfungstrainer für den "
                       "Amateurfunk. Neu aufgebaut von Jan - DC6DX."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("katalog"),
        QStringLiteral("Fragenkatalog (XML), der beim Start geladen werden soll."));
    parser.process(app);

    hrt::MainWindow window;
    window.show();

    const QStringList arguments = parser.positionalArguments();
    if (!arguments.isEmpty()) {
        window.openCatalog(QDir::current().absoluteFilePath(arguments.first()));
    } else {
        window.openInitialCatalog();
    }

    return app.exec();
}