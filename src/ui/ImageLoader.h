// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include <QDir>
#include <QPixmap>
#include <QString>

namespace hrt {

/// Renders the question and answer images of a catalog.
///
/// The BNetzA source stores every image as SVG. Qt's own image plugins cannot
/// rasterise SVG, so without an SVG renderer nothing would be shown at all.
/// The renderer is therefore used explicitly when Qt was built with it, and
/// the result is scaled to a readable width.
///
/// Everything that cannot be loaded yields an empty pixmap instead of an
/// error, because a missing illustration must never stop an examination.
namespace images {

/// Longest edge used when an image is scaled down.
inline constexpr int kMaxWidth = 640;

/// Loads one image, given its path relative to the catalog directory.
/// Returns a null pixmap when the file is missing or unreadable.
QPixmap load(const QDir &catalogDir, const QString &relativePath);

/// True when this build can rasterise SVG.
bool supportsSvg();

} // namespace images
} // namespace hrt