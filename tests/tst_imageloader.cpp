// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "ImageLoader.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QXmlStreamReader>
#include <QtTest>

using namespace hrt;

class ImageLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void questionImageOfTheCatalogRenders();
    void everyImageReferencedByACatalogRenders();
    void rasterImagesAreScaledDownAsWell();
    void missingImageYieldsEmptyPixmap();
    void emptyPathYieldsEmptyPixmap();
    void aDirectoryIsNotAnImage();
};

void ImageLoaderTest::initTestCase()
{
    QVERIFY2(images::supportsSvg(),
             "Dieser Build muss SVG rendern können, sonst bleiben alle 687 Bilder unsichtbar.");
}

/// Reads every image attribute of a catalog file.
static QStringList referencedImages(const QString &catalogPath)
{
    QStringList paths;
    QFile file(catalogPath);
    if (!file.open(QIODevice::ReadOnly))
        return paths;

    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;
        for (const QXmlStreamAttribute &attribute : reader.attributes()) {
            if (attribute.name() == QLatin1String("image"))
                paths.append(attribute.value().toString());
        }
    }
    return paths;
}

void ImageLoaderTest::questionImageOfTheCatalogRenders()
{
    const QDir catalogDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"));
    QVERIFY(catalogDir.exists());

    const QPixmap pixmap = images::load(catalogDir, QStringLiteral("images/NB401_a.svg"));
    QVERIFY2(!pixmap.isNull(), "NB401_a.svg muss gerendert werden");
    QVERIFY(pixmap.width() > 0);
    QVERIFY(pixmap.height() > 0);
    QVERIFY2(pixmap.width() <= images::kMaxWidth,
             qPrintable(QStringLiteral("Breite %1 überschreitet das Limit").arg(pixmap.width())));
}

/// Every illustration a catalog refers to has to be displayable, otherwise the
/// examination would silently drop it.
void ImageLoaderTest::everyImageReferencedByACatalogRenders()
{
    const QDir catalogDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"));
    const QString catalogPath = catalogDir.filePath(QStringLiteral("de-amateurfunk-A.xml"));

    const QStringList paths = referencedImages(catalogPath);
    QVERIFY2(!paths.isEmpty(), "der Katalog A muss Bildverweise enthalten");

    QStringList failures;
    for (const QString &path : paths) {
        if (images::load(catalogDir, path).isNull())
            failures.append(path);
    }

    QVERIFY2(failures.isEmpty(),
             qPrintable(QStringLiteral("%1 von %2 Bildern nicht darstellbar: %3")
                            .arg(failures.size())
                            .arg(paths.size())
                            .arg(failures.mid(0, 5).join(QStringLiteral(", ")))));
}

void ImageLoaderTest::rasterImagesAreScaledDownAsWell()
{
    const QDir catalogDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"));
    const QString rasterPath = catalogDir.filePath(QStringLiteral("images/raster-probe.png"));

    QImage probe(2000, 40, QImage::Format_ARGB32);
    probe.fill(Qt::white);
    {
        QPainter painter(&probe);
        painter.setPen(Qt::black);
        painter.drawRect(0, 0, 1999, 39);
        painter.drawLine(0, 0, 1999, 39);
    }
    QVERIFY(probe.save(rasterPath));

    const QPixmap pixmap = images::load(catalogDir, QStringLiteral("images/raster-probe.png"));
    QVERIFY(!pixmap.isNull());
    QCOMPARE(pixmap.width(), images::kMaxWidth);

    QFile::remove(rasterPath);
}

void ImageLoaderTest::missingImageYieldsEmptyPixmap()
{
    const QDir catalogDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"));
    QVERIFY(images::load(catalogDir, QStringLiteral("images/does-not-exist.svg")).isNull());
    // A traversal out of the catalog must not resolve to an arbitrary file.
    QVERIFY(images::load(catalogDir, QStringLiteral("../schema/catalog.xsd")).isNull());
}

void ImageLoaderTest::emptyPathYieldsEmptyPixmap()
{
    const QDir catalogDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"));
    QVERIFY(images::load(catalogDir, QString()).isNull());
}

void ImageLoaderTest::aDirectoryIsNotAnImage()
{
    const QDir catalogDir(QStringLiteral(HRT_SOURCE_DIR "/data/catalogs"));
    QVERIFY(images::load(catalogDir, QStringLiteral("images")).isNull());
}

QTEST_MAIN(ImageLoaderTest)
#include "tst_imageloader.moc"