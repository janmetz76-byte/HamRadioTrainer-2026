// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ImageLoader.h"

#include <QFileInfo>

#ifdef HRT_HAVE_SVG
#include <QPainter>
#include <QSvgRenderer>
#endif

namespace hrt {
namespace images {

bool supportsSvg()
{
#ifdef HRT_HAVE_SVG
    return true;
#else
    return false;
#endif
}

QPixmap load(const QDir &catalogDir, const QString &relativePath)
{
    if (relativePath.isEmpty())
        return {};

    const QString absolute = QFileInfo(catalogDir.filePath(relativePath)).absoluteFilePath();
    if (!QFileInfo::exists(absolute))
        return {};

#ifdef HRT_HAVE_SVG
    if (absolute.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
        QSvgRenderer renderer(absolute);
        if (!renderer.isValid())
            return {};

        const QSizeF size = renderer.defaultSize();
        const qreal scale = size.width() > kMaxWidth ? qreal(kMaxWidth) / size.width() : 1.0;
        const QSize target(qMax(1, int(size.width() * scale)), qMax(1, int(size.height() * scale)));

        QPixmap pixmap(target);
        if (pixmap.isNull())
            return {};
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();
        return pixmap;
    }
#endif

    QPixmap pixmap(absolute);
    if (pixmap.isNull())
        return {};
    if (pixmap.width() > kMaxWidth)
        pixmap = pixmap.scaledToWidth(kMaxWidth, Qt::SmoothTransformation);
    return pixmap;
}

} // namespace images
} // namespace hrt