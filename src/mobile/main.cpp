// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
//
// Einstieg der Handy-Oberflaeche. Unter Android ist sie die App; am Desktop
// laesst sie sich als "hrt-mobile-preview" in Handygroesse ausprobieren.
#include "ExamRound.h"
#include "StudyRound.h"
#include "Trainer.h"

#include "core/AppIdentity.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    hrt::setApplicationIdentity();

    // Der Stil steht fest in den QML-Dateien ("import QtQuick.Controls.Material"):
    // so packt die APK nur Material und nicht jeden Stil von Qt mit, und die
    // Vorschau am Desktop zeigt dasselbe wie das Handy.

    auto *trainer = new hrt::Trainer(&app);
    qmlRegisterSingletonInstance("HamRadioTrainer.Backend", 1, 0, "Trainer", trainer);
    qmlRegisterUncreatableType<hrt::StudyRound>("HamRadioTrainer.Backend", 1, 0, "StudyRound",
                                                QStringLiteral("gehoert zum Trainer"));
    qmlRegisterUncreatableType<hrt::ExamRound>("HamRadioTrainer.Backend", 1, 0, "ExamRound",
                                               QStringLiteral("gehoert zum Trainer"));

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("HamRadioTrainerUi", "Main");
    return app.exec();
}
