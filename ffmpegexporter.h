#pragma once

class FFmpegExporter
{

public:

    FFmpegExporter();


    // Call this before begining exporting.
    void init(const QString &fileName, bool crop, QRect rect);

    void addframe(const QImage &img, int framenumber);

    // Call this after all the frames were encoded to write them to the file and close it.
    void commitFile();

private:

    QString               fileName;
    bool                  crop          = false;
    QRect                 rect;
    AVFormatContext*      formatContext = nullptr;
    const AVCodec*        codec         = nullptr;
    AVCodecContext*       codecContext  = nullptr;
    const AVOutputFormat* outputFormat  = nullptr;
    AVStream*             stream        = nullptr;
    char* errorstring                   = nullptr;
    int FRAME_RATE                      = 24;
};