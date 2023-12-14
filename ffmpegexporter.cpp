#include "ffmpegexporter.h"

#include <QDebug>

int ERROR_BUFSIZ = 1024;
int64_t nextPts  = 0;

FFmpegExporter::FFmpegExporter()
{
    errorstring = new char[ERROR_BUFSIZ];
}

void FFmpegExporter::init(const QString &fileName, bool crop, const QRect& rect)
{
    int error;
    this->fileName = fileName;

    // Allows you to crop each image with the same rectangle if you want

    this->crop = crop;
    this->rect = rect;

    // Make sure fileName ends with .gif
    // if we get errors here maybe it assigned the wrong codec

    error = avformat_alloc_output_context2(&this->formatContext, NULL, NULL, fileName.toStdString().data());

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << this->errorstring;
        exit(1);
    }

    // Adding the video streams using the default format codecs and initializing the codecs...

    this->outputFormat = this->formatContext->oformat;

    if (this->outputFormat->video_codec != AV_CODEC_ID_NONE)
    {
        // Finding a registered encoder with a matching codec ID...

        this->codec = avcodec_find_encoder(this->outputFormat->video_codec);

        if (this->codec == NULL)
        {
            // It doesn't return an error number

            qWarning() << "Encoder not found";
            exit(1);
        }

        // Adding a new stream to a media file...

        this->stream = avformat_new_stream(this->formatContext, this->codec);

        if (this->stream == NULL)
        {
            // It doesn't return an error number

            qWarning() << "Could not create new AVStream";
            exit(1);
        }

        this->stream->id   = this->formatContext->nb_streams - 1;
        this->codecContext = avcodec_alloc_context3(this->codec);

        if (this->codecContext == NULL)
        {
            // It doesn't return an error number
            qWarning() << "Could not allocate AVContext";
            exit(1);
        }

        switch (this->codec->type)
        {
            case AVMEDIA_TYPE_VIDEO:
            {
                this->codecContext->codec_id  = this->outputFormat->video_codec; // here, outputFormat->video_codec should be AV_CODEC_ID_GIF
                this->codecContext->bit_rate  = 400000;

                this->codecContext->width     = (rect.width() & (~1));
                this->codecContext->height    = (rect.height() & (~1));

                this->codecContext->pix_fmt   = AV_PIX_FMT_RGB8;

                // Timebase: this is the fundamental unit of time (in seconds) in terms of which frame
                // timestamps are represented. For fixed-fps content, timebase should be 1/framerate
                // and timestamp increments should be identical to 1.

                // I needed to multiply this by 3 because for some reason, the GIF was 3x slower than normal.
                // GIF frame rates have a hard limit of 100FPS. If FRAME_RATE=30 i.e. 30FPS then this workaround actually encodes it at 90FPS
                // If however the GIF moves too fast because of this then remove the *3 from the next line.

                this->stream->time_base       = (AVRational){1, FRAME_RATE*3};
                this->codecContext->time_base = this->stream->time_base;

                // Intra frames only, no groups of frames. Bad for compression but I don't want to handle receiving extra frames here,
                // And I don't think GIF codec can even emit groups of frames.

                this->codecContext->gop_size  = 0;
                break;
            }

            case AVMEDIA_TYPE_UNKNOWN:
            case AVMEDIA_TYPE_AUDIO:
            case AVMEDIA_TYPE_DATA:
            case AVMEDIA_TYPE_SUBTITLE:
            case AVMEDIA_TYPE_ATTACHMENT:
            case AVMEDIA_TYPE_NB:
            {
                qWarning() << "Invalid media type "  << this->codec->type;
                exit(1);
                break;
            }
        }

        if (this->formatContext->oformat->flags & AVFMT_GLOBALHEADER)
        {
            this->codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    // This isn't thread-safe.

    error = avcodec_open2(this->codecContext, this->codec, NULL);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << this->errorstring;
        exit(1);
    }

    error = avcodec_parameters_from_context(this->stream->codecpar, this->codecContext);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << this->errorstring;
        exit(1);
    }

    av_dump_format(this->formatContext, 0, fileName.toStdString().data(), 1);

    if (!(this->outputFormat->flags & AVFMT_NOFILE))
    {
        // it should not have AVFMT_NOFILE

        error = avio_open(&this->formatContext->pb, fileName.toStdString().data(), AVIO_FLAG_WRITE);

        if (error < 0)
        {
            av_strerror(error, this->errorstring, ERROR_BUFSIZ);
            qWarning() << this->errorstring;
            exit(1);
        }
    }

    // Writing the stream header, if any...

    error = avformat_write_header(this->formatContext, NULL);

    if      (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << this->errorstring;
        exit(1);
    }
    else if (error == AVSTREAM_INIT_IN_WRITE_HEADER)
    {
        qWarning() << "AVSTREAM_INIT_IN_WRITE_HEADER";  // informational message, not an error
    }
    else if (error == AVSTREAM_INIT_IN_INIT_OUTPUT)
    {
        qWarning() << "AVSTREAM_INIT_IN_INIT_OUTPUT";  // informational message, not an error
    }

}

void FFmpegExporter::addFrame(const QImage &img, int framenumber)
{
    int error;

    // If framenumber is not incremented in sequential order (with first frame having framenumber=1), then this will return without encoding the rest of the frames.
    // This is a relic from my program where I take images from a QOpenGLWindow using the grabFrameBuffer() method and it's important that in the event that OpenGL
    // drops frames, frame encoding is aborted instead of missing dropped frames from the GIF.
    // In particular, QOpenGLWindow paints at 60FPS

    if (nextPts == framenumber)
    {
        return;  // same frame, nothing to do
    }

    nextPts += 1;

    QImage cropped;

    if (this->crop)
    {
        cropped = img.copy(this->rect);
    }
    else
    {
        cropped = img;
    }

    // Make sure that your image and this->rect have the same dimensions

    // Odd numbered GIF widths/heights are known to cause errors in the YUV420P encoding process making the GIF slightly yellow.
    // so the extra pixel length/width is chopped
    const qint32 width  = cropped.width() & (~1);
    const qint32 height = cropped.height() & (~1);

    // Here we need 3 frames for encoding. Basically, the QImage is firstly extracted in AV_PIX_FMT_BGRA.
    // We need then to convert it to AV_PIX_FMT_RGB8 which is required by the .gif format.
    // If we do that directly, there will be some artefacts and bad effects... to prevent that
    // we convert FIRST AV_PIX_FMT_BGRA into AV_PIX_FMT_YUV420P THEN into AV_PIX_FMT_RGB8.

    AVFrame * frame = av_frame_alloc();
    frame->width = this->codecContext->width;
    frame->height = this->codecContext->height;
    frame->format = this->codecContext->pix_fmt;

    AVFrame * tmpFrame = av_frame_alloc();
    tmpFrame->width = this->codecContext->width;
    tmpFrame->height = this->codecContext->height;
    tmpFrame->format = AV_PIX_FMT_BGRA;

    AVFrame * yuvFrame = av_frame_alloc();
    yuvFrame->width = codecContext->width;
    yuvFrame->height = codecContext->height;
    yuvFrame->format = AV_PIX_FMT_YUV420P;

    error = av_frame_get_buffer(tmpFrame, 0);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << " " << this->errorstring;
        exit(1);
    }

    error = av_image_alloc(tmpFrame->data, tmpFrame->linesize, width, height, AV_PIX_FMT_BGRA, 32);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << " " << this->errorstring;
        exit(1);
    }

    // When we pass a frame to the encoder, it may keep a reference to it internally;
    // make sure we do not overwrite it here!
    error = av_frame_make_writable(tmpFrame);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << " " << this->errorstring;
        exit(1);
    }

    // Converting QImage to AV_PIX_FMT_BGRA AVFrame ... 

    for (qint32 y = 0; y < height; y++)
    {
        const uint8_t * scanline = cropped.scanLine(y);

        for (qint32 x = 0; x < width * 4; x++)
        {
            tmpFrame->data[0][y * tmpFrame->linesize[0] + x] = scanline[x];
        }
    }

    // Make sure to clear the frame. It prevents a bug that displays only the
    // first captured frame on the GIF export.

    if (frame)
    {
        av_frame_free(&frame);
        frame = Q_NULLPTR;
    }

    frame         = av_frame_alloc();
    frame->width  = codecContext->width;
    frame->height = codecContext->height;
    frame->format = codecContext->pix_fmt;

    error = av_image_alloc(frame->data, frame->linesize, width, height, codecContext->pix_fmt, 32);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << " " << this->errorstring;
        exit(1);
    }


    if (yuvFrame)
    {
        av_frame_free(&yuvFrame);
        yuvFrame = Q_NULLPTR;
    }

    yuvFrame         = av_frame_alloc();
    yuvFrame->width  = codecContext->width;
    yuvFrame->height = codecContext->height;
    yuvFrame->format = AV_PIX_FMT_YUV420P;

    error = av_image_alloc(yuvFrame->data, yuvFrame->linesize, width, height, AV_PIX_FMT_YUV420P, 32);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << " " << this->errorstring;
        exit(1);
    }

    // Converting BGRA -> YUV420P...

    struct SwsContext * swsCtx = Q_NULLPTR;
    struct SwsContext * swsGIFCtx = Q_NULLPTR;

    if (!swsCtx)
    {
        swsCtx = sws_getContext(width, height,
                                AV_PIX_FMT_BGRA,
                                width, height,
                                AV_PIX_FMT_YUV420P,
                                SWS_BICUBIC, NULL, NULL, NULL);
        if (swsCtx == NULL)
        {
            // It doesn't return an error number
            qWarning() << " " << "Could not allocate SwsContext swsCtx";
            exit(1);
        }
    }

    // ...then converting YUV420P -> RGB8 (natif GIF format pixel)

    if (!swsGIFCtx)
    {
        swsGIFCtx = sws_getContext(width, height,
                                    AV_PIX_FMT_YUV420P,
                                    codecContext->width, codecContext->height,
                                    codecContext->pix_fmt,
                                    SWS_BICUBIC, NULL, NULL, NULL);
        if (swsGIFCtx == NULL)
        {
            // It doesn't return an error number
            qWarning() << "Could not allocate SwsContext swsGIFCtx";
            exit(1);
        }
    }

    // This double scaling prevent some artifacts on the GIF and improve
    // significantly the display quality

    sws_scale(swsCtx,
              (const uint8_t * const *)tmpFrame->data,
              tmpFrame->linesize,
              0,
              codecContext->height,
              yuvFrame->data,
              yuvFrame->linesize);

    sws_scale(swsGIFCtx,
              (const uint8_t * const *)yuvFrame->data,
              yuvFrame->linesize,
              0,
              codecContext->height,
              frame->data,
              frame->linesize);

    AVPacket *packet = av_packet_alloc();

    if (packet == NULL)
    {
        // It doesn't return an error number

        qWarning() << "Could not allocate AVPacket";
        exit(1);
    }

    av_init_packet(packet);

    // Packet data will be allocated by the encoder

    packet->data = NULL;
    packet->size = 0;
    frame->pts   = nextPts++; // nextPts starts at 0

    error = avcodec_send_frame(this->codecContext, frame);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << this->errorstring;
        exit(1);
    }

    error = avcodec_receive_packet(this->codecContext, packet);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << this->errorstring;
        exit(1);
    }

    // Rescale output packet timestamp values from codec to stream timebase

    av_packet_rescale_ts(packet, codecContext->time_base, this->stream->time_base);
    packet->stream_index = this->stream->index;

    // Write the compressed frame to the media file.

    error = av_interleaved_write_frame(formatContext, packet);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << this->errorstring;
        exit(1);
    }

    // free packets

    av_freep(&tmpFrame->data[0]);
    av_freep(&yuvFrame->data[0]);
    av_freep(&frame->data[0]);

    av_frame_free(&frame);
    av_frame_free(&tmpFrame);
    av_frame_free(&yuvFrame);
    sws_freeContext(swsCtx);
    sws_freeContext(swsGIFCtx);

    av_packet_free(&packet);
}

void FFmpegExporter::commitFile()
{
    int error;
    nextPts = 0;

    error = av_write_trailer(this->formatContext);

    if (error < 0)
    {
        av_strerror(error, this->errorstring, ERROR_BUFSIZ);
        qWarning() << this->errorstring;
        exit(1);
    }

    avcodec_free_context(&this->codecContext);

    if (!(this->outputFormat->flags & AVFMT_NOFILE))
    {
        // Closing the output file...

        error = avio_closep(&this->formatContext->pb);

        if (error < 0)
        {
            av_strerror(error, this->errorstring, ERROR_BUFSIZ);
            qWarning() << this->errorstring;
            exit(1);
        }
    }

    avformat_free_context(this->formatContext);
}
