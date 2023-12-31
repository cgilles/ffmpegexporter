/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2023-12-15
 * Description : a command line tool to export images to video stream with FFMPEG
 *
 * Copyright (C) 2011-2023 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

// Qt includes

#include <QFile>
#include <QDir>
#include <QImage>
#include <QDebug>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QCoreApplication>

#include "ffmpegexporter.h"

// Local includes

int main(int argc, char** argv)
{
    QCoreApplication(argc, argv);

    if (argc != 3)
    {
        qInfo() << "ffmpegexporter - Load images list from a directory to convert in video stream";
        qInfo() << "Usage: <input_directory> <output_video_file>";
        return -1;
    }

    QString inputPath(QString::fromUtf8(argv[1]));
    QString outputFile(QString::fromUtf8(argv[2]));

    QDir dir(inputPath);
    QStringList images = dir.entryList(QStringList() << "*.jpg" << "*.JPG", QDir::Files);

    if (images.isEmpty())
    {
        qDebug() << "List of images is empty";
        return 0;
    }

    FFmpegExporter converter;
    converter.init(outputFile, true, QRect(0, 0, 800, 600));
    int i = 0;

    foreach (const QString& filename, images)
    {
        qDebug() << "Processing" << filename;
        QImage img(filename);
        converter.addFrame(img, i++);
    }

    converter.commitFile();

    return 0;
}
