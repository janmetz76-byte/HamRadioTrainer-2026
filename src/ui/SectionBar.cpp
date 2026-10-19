// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
#include "SectionBar.h"

#include <QHBoxLayout>
#include <QLabel>

namespace hrt {

SectionBar::SectionBar(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    // Der negative untere Rand zieht den Balken an den Block darunter. Ohne
    // das klafft zwischen Leiste und Inhalt eine leere Zeile, und die Leiste
    // wirkt, als gehoere sie zu gar nichts.
    layout->setContentsMargins(8, 3, 8, -4);
    layout->setSpacing(0);

    m_title = new QLabel(title, this);
    QFont schrift = m_title->font();
    schrift.setBold(true);
    m_title->setFont(schrift);
    // Der Text bleibt fuer Screenreader und fuer das Kopieren lesbar; nur die
    // Darstellung wird zum Band.
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_title);

    // Die Farben kommen aus der Palette und nicht als feste Werte: sonst
    // waere das Band in einem dunklen Fenster ein heller Fleck.
    setAutoFillBackground(true);
    QPalette eigene = palette();
    eigene.setColor(QPalette::Window, eigene.color(QPalette::AlternateBase));
    setPalette(eigene);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void SectionBar::setTitle(const QString &title)
{
    m_title->setText(title);
}

} // namespace hrt
