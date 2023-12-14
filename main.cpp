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
#include <QImage>
#include <QDebug>
#include <QByteArray>
#include <QCoreApplication>

#include "ffmpegexporter.h"

// Local includes

int main(int argc, char** argv)
{
    QCoreApplication(argc, argv);

    if (argc != 2)
    {
        qInfo() << "ffmpegexporter - Load images list to convert in video stream";
        qInfo() << "Usage: <output_video_file> <input_image_files>";
        return -1;
    }

    FFmpegExporter converter;

    return 0;
}
