// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#pragma once

namespace hrt {

/// Name und Version, unter denen das Programm seine Einstellungen ablegt.
///
/// Steht hier und nicht in main(), weil die Tests dieselben Namen brauchen.
/// Ohne sie schreibt QSettings unter Windows in einen unbrauchbaren
/// Registry-Zweig und unter Linux in eine INI-Datei: dass die Einstellungen
/// beim Start wieder da sind, laesst sich dann nur auf einer Plattform
/// nachweisen - genau das ist schon einmal schiefgegangen.
void setApplicationIdentity();

} // namespace hrt