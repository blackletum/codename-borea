#ifndef VIDEOENGINE_H
#define VIDEOENGINE_H

extern "C" {

#include "ffmpeg/libavcodec/avcodec.h"
#include "ffmpeg/libavformat/avformat.h"

#include "ffmpeg/libavutil/avutil.h"
#include "ffmpeg/libavutil/channel_layout.h"
#include "ffmpeg/libswscale/swscale.h"

#include "ffmpeg/libswresample/swresample.h"

}

struct VideoData {
    // Public things for other parts of the program to read from
    int width, height;
    AVRational time_base;
    double pts;

    // Private internal state
    AVFormatContext* av_format_ctx;
    AVCodecContext* av_codec_ctx;
    int video_stream_index;
    AVFrame* av_frame;
    AVPacket* av_packet;
    SwsContext* sws_scaler_ctx;
};

class CVideoEngine
{
public:
	void Init();
    void VidInit();
	void LoadVideo(const char* video_path);
	void DrawVideo(float flTime);
    VideoData viddata;
    bool videoended;

};

#ifndef VIDEOENGINE_CPP
extern CVideoEngine gVideoEngine;
#endif

#endif