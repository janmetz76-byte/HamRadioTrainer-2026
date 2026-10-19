// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "core/AppIdentity.h"

#include <QCoreApplication>

namespace hrt {

void setApplicationIdentity()
{
    QCoreApplication::setOrganizationName(QStringLiteral("HamRadioTrainer"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("hamradiotrainer.org"));
    QCoreApplication::setApplicationName(QStringLiteral("HamRadioTrainer 2026"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0-dev"));
}

} // namespace hrt