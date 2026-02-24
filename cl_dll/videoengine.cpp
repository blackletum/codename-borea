#define VIDEOENGINE_CPP
#include "hud.h"

#include "bsprenderer.h"
#include "rendererdefs.h"
#include "videoengine.h"

#include "cl_util.h"

#include "openal/OpenAL_System.h"

#include "opengl_utils/GL_TextureHandler.h"
#include "opengl_utils/GL_Buffers.h"
#include "opengl_utils/GL_VertexArrayObject.h"
#include "opengl_utils/GL_ShaderProgram.h"

double start_time = 0;

bool first_init = true;

float scale_X, scale_Y;

GL_VertexArrayObject* m_pVideoVAO;
GL_BufferHandler* m_pVideoQuad;

cvar_t* ffmpeg_soundvolume = nullptr;

///////////////
//ffmpeg start
///////////////


//avformat
typedef AVFormatContext* (*avformat_alloc_context_func)(void);
typedef int (*avformat_open_input_func)(AVFormatContext**, const char*, const AVInputFormat*, AVDictionary**);
typedef int (*av_read_frame_func)(AVFormatContext*, AVPacket*);
typedef void (*avformat_close_input_func)(AVFormatContext**);
typedef int (*av_seek_frame_func)(AVFormatContext* s, int stream_index, int64_t timestamp, int flags);

avformat_alloc_context_func avformat_alloc_context_;
avformat_open_input_func avformat_open_input_;
avformat_close_input_func avformat_close_input_;
av_read_frame_func av_read_frame_;
av_seek_frame_func av_seek_frame_;

//avcodec
typedef const AVCodec* (*avcodec_find_decoder_func)(enum AVCodecID);
typedef AVCodecContext* (*avcodec_alloc_context3_func)(const AVCodec*);
typedef int (*avcodec_parameters_to_context_func)(AVCodecContext*, const struct AVCodecParameters*);
typedef int (*avcodec_open2_func)(AVCodecContext*, const AVCodec*, AVDictionary**);
typedef int (*avcodec_send_packet_func)(AVCodecContext*, const AVPacket*);
typedef int (*avcodec_receive_frame_func)(AVCodecContext*, AVFrame*);
typedef void (*avcodec_free_context_func)(AVCodecContext**);
typedef void (*av_packet_unref_func)(AVPacket*);
typedef AVPacket* (*av_packet_alloc_func)(void);
typedef void (*av_packet_free_func)(AVPacket**);

avcodec_find_decoder_func avcodec_find_decoder_;
avcodec_alloc_context3_func avcodec_alloc_context3_;
avcodec_parameters_to_context_func avcodec_parameters_to_context_;
avcodec_open2_func avcodec_open2_;
avcodec_send_packet_func avcodec_send_packet_;
avcodec_receive_frame_func avcodec_receive_frame_;
avcodec_free_context_func avcodec_free_context_;

av_packet_unref_func av_packet_unref_;
av_packet_alloc_func av_packet_alloc_;
av_packet_free_func av_packet_free_;

//avutil
typedef AVFrame* (*av_frame_alloc_func)(void);
typedef void (*av_frame_free_func)(AVFrame**);
typedef int (*av_strerror_func)(int, char*, size_t);
typedef void (*av_channel_layout_default_func)(AVChannelLayout*, int);
typedef void (*av_channel_layout_uninit_func)(AVChannelLayout* channel_layout);
typedef int64_t (*av_rescale_rnd_func)(int64_t a, int64_t b, int64_t c, enum AVRounding rnd);
typedef int (*av_samples_alloc_func)(uint8_t** audio_data, int* linesize, int nb_channels,
    int nb_samples, enum AVSampleFormat sample_fmt, int align);
typedef int (*av_samples_get_buffer_size_func)(int* linesize, int nb_channels, int nb_samples,
    enum AVSampleFormat sample_fmt, int align);
typedef void (*av_freep_func)(void* ptr);

av_frame_alloc_func av_frame_alloc_;
av_frame_free_func av_frame_free_;
av_strerror_func av_strerror_;
av_channel_layout_default_func av_channel_layout_default_;
av_channel_layout_uninit_func av_channel_layout_uninit_;
av_rescale_rnd_func av_rescale_rnd_;
av_samples_alloc_func av_samples_alloc_;
av_samples_get_buffer_size_func av_samples_get_buffer_size_;
av_freep_func av_freep_;

//swscale

typedef struct SwsContext* (*sws_getContext_func)(int srcW, int srcH, enum AVPixelFormat srcFormat,
    int dstW, int dstH, enum AVPixelFormat dstFormat,
    int flags, SwsFilter* srcFilter,
    SwsFilter* dstFilter, const double* param);
typedef void (*sws_freeContext_func)(struct SwsContext* swsContext);
typedef int (*sws_scale_func)(struct SwsContext* c, const uint8_t* const srcSlice[],
    const int srcStride[], int srcSliceY, int srcSliceH,
    uint8_t* const dst[], const int dstStride[]);

sws_getContext_func sws_getContext_;
sws_freeContext_func sws_freeContext_;
sws_scale_func sws_scale_;

//swresample

typedef int (*swr_alloc_set_opts2_func)(struct SwrContext** ps,
             const AVChannelLayout* out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate,
             const AVChannelLayout* in_ch_layout, enum AVSampleFormat  in_sample_fmt, int  in_sample_rate,
             int log_offset, void* log_ctx);
typedef int (*swr_init_func)(struct SwrContext* s);
typedef int (*swr_free_func)(struct SwrContext** s);
typedef int64_t(*swr_get_delay_func)(struct SwrContext* s, int64_t base);
typedef int (*swr_convert_func)(struct SwrContext* s, uint8_t* const* out, int out_count,
    const uint8_t* const* in, int in_count);

swr_alloc_set_opts2_func swr_alloc_set_opts2_;
swr_init_func swr_init_;
swr_free_func swr_free_;
swr_get_delay_func swr_get_delay_;
swr_convert_func swr_convert_;

//ffmpeg end

CVideoEngine gVideoEngine;

GL_TextureHandler* m_pVideoTexture = nullptr;

uint8_t *frame_data = nullptr;

static const char* av_make_error(int errnum) {
    static char str[AV_ERROR_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    av_strerror_(errnum, str, AV_ERROR_MAX_STRING_SIZE);
    return str;
}

static AVPixelFormat correct_for_deprecated_pixel_format(AVPixelFormat pix_fmt) {
    // Fix swscaler deprecated pixel format warning
    // (YUVJ has been deprecated, change pixel format to regular YUV)
    switch (pix_fmt) {
    case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P;
    case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P;
    case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P;
    case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P;
    default:                  return pix_fmt;
    }
}

void video_reader_close(VideoData* viddata) {
    if (viddata->sws_scaler_ctx) {
        sws_freeContext_(viddata->sws_scaler_ctx);
        viddata->sws_scaler_ctx = nullptr;
    }

    if (viddata->av_frame) {
        av_frame_free_(&viddata->av_frame);
        viddata->av_frame = nullptr;
    }

    if (viddata->av_packet) {
        av_packet_free_(&viddata->av_packet);
        viddata->av_packet = nullptr;
    }

    if (viddata->av_codec_ctx) {
        avcodec_free_context_(&viddata->av_codec_ctx);
        viddata->av_codec_ctx = nullptr;
    }

    if (viddata->av_format_ctx) {
        avformat_close_input_(&viddata->av_format_ctx);
        viddata->av_format_ctx = nullptr;
    }

    viddata->width = 0;
    viddata->height = 0;
    viddata->video_stream_index = -1;
}

// read a video frame
bool video_reader_readframe(VideoData* viddata, uint8_t* frame_buffer) {
    auto& width = viddata->width;
    auto& height = viddata->height;
    auto& time_base = viddata->time_base;
    auto& av_format_ctx = viddata->av_format_ctx;
    auto& av_codec_ctx = viddata->av_codec_ctx;
    auto& video_stream_index = viddata->video_stream_index;
    auto& av_frame = viddata->av_frame;
    auto& av_packet = viddata->av_packet;
    auto& sws_scaler_ctx = viddata->sws_scaler_ctx;

    bool noframeleft = true;


    // Decode one frame
    int response;
    while (av_read_frame_(av_format_ctx, av_packet) >= 0) {
        noframeleft = false;
        if (av_packet->stream_index != video_stream_index) {
            av_packet_unref_(av_packet);
            continue;
        }

        response = avcodec_send_packet_(av_codec_ctx, av_packet);
        if (response < 0) {
            printf("Failed to decode packet: %s\n", av_make_error(response));
            return false;
        }

        response = avcodec_receive_frame_(av_codec_ctx, av_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_packet_unref_(av_packet);
            continue;
        }
        else if (response < 0) {
            printf("Failed to decode packet: %s\n", av_make_error(response));
            return false;
        }

        av_packet_unref_(av_packet);
        break;
    }

    if (noframeleft)
        return false;

    viddata->pts = av_frame->pts * (double)viddata->time_base.num / (double)viddata->time_base.den;

    // Set up sws scaler
    if (!sws_scaler_ctx) {
        auto source_pix_fmt = correct_for_deprecated_pixel_format(av_codec_ctx->pix_fmt);
        sws_scaler_ctx = sws_getContext_(width, height, source_pix_fmt,
            width, height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, NULL, NULL, NULL);
    }
    if (!sws_scaler_ctx) {
        printf("Couldn't initialize sw scaler\n");
        return false;
    }

    uint8_t* dest[4] = { frame_buffer, NULL, NULL, NULL };
    int dest_linesize[4] = { width * 4, 0, 0, 0 };
    sws_scale_(sws_scaler_ctx, av_frame->data, av_frame->linesize, 0, av_frame->height, dest, dest_linesize);

    return true;
}

bool video_reader_loadaudio(VideoData* viddata, const char* filepath)
{
    auto& width = viddata->width;
    auto& height = viddata->height;
    auto& time_base = viddata->time_base;
    auto& av_format_ctx = viddata->av_format_ctx;
    auto& av_codec_ctx = viddata->av_codec_ctx;
    auto& video_stream_index = viddata->video_stream_index;
    auto& av_frame = viddata->av_frame;
    auto& av_packet = viddata->av_packet;

    int audio_stream_index = -1;
    AVCodecParameters* audio_codec_params = nullptr;
    AVCodec* audio_codec = nullptr;

    for (unsigned int i = 0; i < av_format_ctx->nb_streams; ++i) {
        AVCodecParameters* codec_params = av_format_ctx->streams[i]->codecpar;
        if (codec_params->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_codec_params = codec_params;
            audio_codec = const_cast<AVCodec*>(avcodec_find_decoder_(codec_params->codec_id));
            audio_stream_index = i;
            break;
        }
    }

    AVCodecContext* audio_codec_ctx = avcodec_alloc_context3_(audio_codec);
    avcodec_parameters_to_context_(audio_codec_ctx, audio_codec_params);
    avcodec_open2_(audio_codec_ctx, audio_codec, NULL);

    AVPacket* packet = av_packet_alloc_();
    AVFrame* frame = av_frame_alloc_();

    SwrContext* swr = nullptr;
    AVChannelLayout channel1;
    AVChannelLayout channel2 ;
    av_channel_layout_default_(&channel1, 2);
    av_channel_layout_default_(&channel2, audio_codec_ctx->ch_layout.nb_channels);
    swr_alloc_set_opts2_(
        &swr,
        &channel1,  // Output: stereo
        AV_SAMPLE_FMT_S16,                 // Output: signed 16-bit
        audio_codec_ctx->sample_rate,
        &channel2,
        audio_codec_ctx->sample_fmt,
        audio_codec_ctx->sample_rate,
        0, NULL);
    swr_init_(swr);

    std::vector<uint8_t> audio_data;

    while (av_read_frame_(av_format_ctx, packet) >= 0) {
        if (packet->stream_index == audio_stream_index) {
            if (avcodec_send_packet_(audio_codec_ctx, packet) == 0) {
                while (avcodec_receive_frame_(audio_codec_ctx, frame) == 0) {
                    uint8_t* out_data;
                    int out_linesize;
                    int out_samples = av_rescale_rnd_(swr_get_delay_(swr, audio_codec_ctx->sample_rate) + frame->nb_samples,
                        audio_codec_ctx->sample_rate, audio_codec_ctx->sample_rate, AV_ROUND_UP);
                    av_samples_alloc_(&out_data, &out_linesize, 2, out_samples, AV_SAMPLE_FMT_S16, 0);
                    int samples_converted = swr_convert_(swr, &out_data, out_samples,
                        (const uint8_t**)frame->data, frame->nb_samples);

                    int buffer_size = av_samples_get_buffer_size_(NULL, 2, samples_converted, AV_SAMPLE_FMT_S16, 1);

                    audio_data.insert(audio_data.end(), out_data, out_data + buffer_size);

                    av_freep_(&out_data);
                }
            }
        }
        av_packet_unref_(packet);
    }

    gSoundSystem.StartSound_rawdata(filepath, audio_data.data(), audio_data.size(), audio_codec_ctx->sample_rate, ffmpeg_soundvolume->value);

    av_seek_frame_(av_format_ctx, -1, 0, AVSEEK_FLAG_BACKWARD);

    swr_free_(&swr);

    avcodec_free_context_(&audio_codec_ctx);

    av_packet_free_(&packet);
    av_frame_free_(&frame);

    av_channel_layout_uninit_(&channel1);
    av_channel_layout_uninit_(&channel2);
    return true;
}

bool video_reader_open(VideoData* viddata, const char* filepath)
{
    // Unpack members of state
    auto& width = viddata->width;
    auto& height = viddata->height;
    auto& time_base = viddata->time_base;
    auto& av_format_ctx = viddata->av_format_ctx;
    auto& av_codec_ctx = viddata->av_codec_ctx;
    auto& video_stream_index = viddata->video_stream_index;
    auto& av_frame = viddata->av_frame;
    auto& av_packet = viddata->av_packet;

    // Open the file using libavformat
    av_format_ctx = avformat_alloc_context_();
    if (!av_format_ctx) {
        printf("Couldn't created AVFormatContext\n");
        return false;
    }

    if (avformat_open_input_(&av_format_ctx, filepath, NULL, NULL) != 0) {
        printf("Couldn't open video file\n");
        return false;
    }

    // Find the first valid video stream inside the file
    video_stream_index = -1;
    AVCodecParameters* av_codec_params = nullptr;
    AVCodec* av_codec = nullptr;
    for (int i = 0; i < av_format_ctx->nb_streams; ++i) {
        av_codec_params = av_format_ctx->streams[i]->codecpar;
        av_codec = const_cast<AVCodec*>(avcodec_find_decoder_(av_codec_params->codec_id));
        if (!av_codec) {
            continue;
        }
        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            width = av_codec_params->width;
            height = av_codec_params->height;
            time_base = av_format_ctx->streams[i]->time_base;
            break;
        }
    }
    if (video_stream_index == -1) {
        printf("Couldn't find valid video stream inside file\n");
        return false;
    }

    // Set up a codec context for the decoder
    av_codec_ctx = avcodec_alloc_context3_(av_codec);
    if (!av_codec_ctx) {
        printf("Couldn't create AVCodecContext\n");
        return false;
    }
    if (avcodec_parameters_to_context_(av_codec_ctx, av_codec_params) < 0) {
        printf("Couldn't initialize AVCodecContext\n");
        return false;
    }
    if (avcodec_open2_(av_codec_ctx, av_codec, NULL) < 0) {
        printf("Couldn't open codec\n");
        return false;
    }

    av_frame = av_frame_alloc_();
    if (!av_frame) {
        printf("Couldn't allocate AVFrame\n");
        return false;
    }
    av_packet = av_packet_alloc_();
    if (!av_packet) {
        printf("Couldn't allocate AVPacket\n");
        return false;
    }

    return video_reader_loadaudio(viddata, filepath);
}

void InitFFMPEG()
{
    char buffer[256];
    const char* gamedir = gEngfuncs.pfnGetGameDirectory();
    char fullPath[512];
    sprintf_s(buffer, "./%s/cl_dlls/ffmpeg/", gamedir);
    GetFullPathNameA(buffer, 512, fullPath, NULL);
    SetDllDirectoryA(fullPath);

    // i tried using goldsrc's ffmpeg libraries, but aside from being ancient versions, they didnt
    // even work, so we'll have to use our own dlls. wish i could use static libraries instead
    // but compiling ffmpeg static libraries is damn near impossible on windows.
    //
    // salsatobias: todo: load the dlls using window's delay load dll because this is so ugly

    sprintf_s(buffer, "%s/cl_dlls/ffmpeg/avcodec-62.dll", gamedir);

    HMODULE avcodec = LoadLibraryA(buffer);

    sprintf_s(buffer, "%s/cl_dlls/ffmpeg/avutil-60.dll", gamedir);

    HMODULE avutil = LoadLibraryA(buffer);

    sprintf_s(buffer, "%s/cl_dlls/ffmpeg/avformat-62.dll", gamedir);

    HMODULE avformat = LoadLibraryA(buffer);

    sprintf_s(buffer, "%s/cl_dlls/ffmpeg/swscale-9.dll", gamedir);

    HMODULE swscale = LoadLibraryA(buffer);

    sprintf_s(buffer, "%s/cl_dlls/ffmpeg/swresample-6.dll", gamedir);

    HMODULE swresample = LoadLibraryA(buffer);

    //avformat

    avformat_alloc_context_ = (avformat_alloc_context_func)GetProcAddress(avformat, "avformat_alloc_context");
    avformat_open_input_ = (avformat_open_input_func)GetProcAddress(avformat, "avformat_open_input");
    avformat_close_input_ = (avformat_close_input_func)GetProcAddress(avformat, "avformat_close_input");
    av_read_frame_ = (av_read_frame_func)GetProcAddress(avformat, "av_read_frame");
    av_seek_frame_ = (av_seek_frame_func)GetProcAddress(avformat, "av_seek_frame");

    //avcodec

    avcodec_find_decoder_ = (avcodec_find_decoder_func)GetProcAddress(avcodec, "avcodec_find_decoder");
    avcodec_alloc_context3_ = (avcodec_alloc_context3_func)GetProcAddress(avcodec, "avcodec_alloc_context3");
    avcodec_parameters_to_context_ = (avcodec_parameters_to_context_func)GetProcAddress(avcodec, "avcodec_parameters_to_context");
    avcodec_open2_ = (avcodec_open2_func)GetProcAddress(avcodec, "avcodec_open2");
    avcodec_send_packet_ = (avcodec_send_packet_func)GetProcAddress(avcodec, "avcodec_send_packet");
    avcodec_receive_frame_ = (avcodec_receive_frame_func)GetProcAddress(avcodec, "avcodec_receive_frame");
    avcodec_free_context_ = (avcodec_free_context_func)GetProcAddress(avcodec, "avcodec_free_context");
    av_packet_unref_ = (av_packet_unref_func)GetProcAddress(avcodec, "av_packet_unref");
    av_packet_alloc_ = (av_packet_alloc_func)GetProcAddress(avcodec, "av_packet_alloc");
    av_packet_free_ = (av_packet_free_func)GetProcAddress(avcodec, "av_packet_free");

    //avutil

    av_frame_alloc_ = (av_frame_alloc_func)GetProcAddress(avutil, "av_frame_alloc");
    av_frame_free_ = (av_frame_free_func)GetProcAddress(avutil, "av_frame_free");
    av_strerror_ = (av_strerror_func)GetProcAddress(avutil, "av_strerror");
    av_channel_layout_default_ = (av_channel_layout_default_func)GetProcAddress(avutil, "av_channel_layout_default");;
    av_channel_layout_uninit_ = (av_channel_layout_uninit_func)GetProcAddress(avutil, "av_channel_layout_uninit");;
    av_rescale_rnd_ = (av_rescale_rnd_func)GetProcAddress(avutil, "av_rescale_rnd");
    av_freep_ = (av_freep_func)GetProcAddress(avutil, "av_freep");
    av_samples_alloc_ = (av_samples_alloc_func)GetProcAddress(avutil, "av_samples_alloc");
    av_samples_get_buffer_size_ = (av_samples_get_buffer_size_func)GetProcAddress(avutil, "av_samples_get_buffer_size");

    //swscale

    sws_getContext_ = (sws_getContext_func)GetProcAddress(swscale, "sws_getContext");
    sws_freeContext_ = (sws_freeContext_func)GetProcAddress(swscale, "sws_freeContext");
    sws_scale_ = (sws_scale_func)GetProcAddress(swscale, "sws_scale");

    //swresample (audio)

    swr_alloc_set_opts2_ = (swr_alloc_set_opts2_func)GetProcAddress(swresample, "swr_alloc_set_opts2");
    swr_init_ = (swr_init_func)GetProcAddress(swresample, "swr_init");
    swr_free_ = (swr_free_func)GetProcAddress(swresample, "swr_free");
    swr_get_delay_ = (swr_get_delay_func)GetProcAddress(swresample, "swr_get_delay");
    swr_convert_ = (swr_convert_func)GetProcAddress(swresample, "swr_convert");

    SetDllDirectoryA("");

    ffmpeg_soundvolume = gEngfuncs.pfnRegisterVariable("ffmpeg_soundvolume", "75", FCVAR_ARCHIVE);

    first_init = false;
}

void drawFrame(VideoData* data) {

    // clear viewport

    glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ZERO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gBSPRenderer.m_FilterShader.Bind();
    gBSPRenderer.m_FilterShader.Uniform1i(gBSPRenderer.m_FilterShader.GetUniformLoc("gaussian_pass"), 0);
    gBSPRenderer.m_FilterShader.Uniform1i(gBSPRenderer.m_FilterShader.GetUniformLoc("flipped"), 1);

    m_pVideoVAO->BindVAO();

    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    GL_VertexArrayObject::ResetVAOBinding();

    gBSPRenderer.m_FilterShader.Uniform1i(gBSPRenderer.m_FilterShader.GetUniformLoc("flipped"), 0);

    GL_ShaderProgram::ResetShaderBind();
}

void CVideoEngine::ClearVideo()
{
    video_reader_close(&viddata);
    start_time = 0;
    videoended = true;
    viddata.width = viddata.height = viddata.pts = 0;
    viddata.av_format_ctx = nullptr;
    viddata.av_codec_ctx = nullptr;
    viddata.video_stream_index = -1;
    viddata.av_frame = nullptr;
    viddata.av_packet = nullptr;
    viddata.sws_scaler_ctx = nullptr;
}

void CVideoEngine::Init()
{
    InitFFMPEG();
}

void CVideoEngine::VidInit()
{
    ClearVideo();

    videoended = true;

    if (m_pVideoTexture)
    {
        delete m_pVideoTexture;
        m_pVideoTexture = nullptr;
    }
    gSoundSystem.StopSounds(true);
}

void CVideoEngine::LoadVideo(const char* video_path)
{
    ClearVideo();

    if (!strcmp(video_path, "STOP"))
    {
        if (m_pVideoTexture)
        {
            delete m_pVideoTexture;
            m_pVideoTexture = nullptr;
        }
        gSoundSystem.StopSounds(false);

        return;
    }

    videoended = false;

    char fullpath[256];
    sprintf_s(fullpath, "%s/%s", gEngfuncs.pfnGetGameDirectory(), video_path);
    if (!video_reader_open(&viddata, fullpath))
        return;

    constexpr int ALIGNMENT = 128;
    const int frame_width = viddata.width;
    const int frame_height = viddata.height;
    if (frame_data)
    {
        _aligned_free(frame_data);
        frame_data = nullptr;
    }

    frame_data = (uint8_t*)_aligned_malloc(frame_width * frame_height * 4, ALIGNMENT);
    if (!frame_data) {
        gEngfuncs.Con_DPrintf("Couldn't allocate frame buffer\n");
        return;
    }

    if (m_pVideoTexture)
        delete m_pVideoTexture;

    GL_TextureHandler::gl_texturecreationinfo_t texinfo;
    texinfo.SetInfo(std::string("ffmpegvideoframe"), GL_TextureHandler::_2DTexture, GL_RGB, viddata.width, viddata.height, 0, GL_RGB, GL_UNSIGNED_BYTE);

    m_pVideoTexture = new GL_TextureHandler(&texinfo);

    float videoAspect = (float)viddata.width / (float)viddata.height;
    float screenAspect = (float)ScreenWidth / (float)ScreenHeight;

    scale_X = 1.0f;
    scale_Y = 1.0f;

    if (videoAspect > screenAspect) {
        scale_Y = screenAspect / videoAspect;
    }
    else {
        scale_X = videoAspect / screenAspect;
    }

    //glVertex2f(-scale_X, -scale_Y)
    //glVertex2f(scale_X, -scale_Y);
    //glVertex2f(scale_X, scale_Y);
    //glVertex2f(-scale_X, scale_Y);

    Vector verts[] =
    {
        Vector(scale_X, scale_Y, 0.0),		// Top-right
        Vector(-scale_X, scale_Y, 0.0),		// Top-left
        Vector(-scale_X, -scale_Y, 0.0),		// Bottom-left

        Vector(-scale_X, -scale_Y, 0.0),		// Bottom-left
        Vector(scale_X, -scale_Y, 0.0),		// Bottom-right
        Vector(scale_X, scale_Y, 0.0),		// Top-right
    };

    if (m_pVideoVAO)
    {
        delete m_pVideoVAO;
        delete  m_pVideoQuad;
    }

    m_pVideoVAO = new GL_VertexArrayObject();
    m_pVideoVAO->BindVAO();

    m_pVideoQuad = new GL_BufferHandler();

    m_pVideoQuad->Bind(GL_BufferHandler::ArrayBuffer);
    m_pVideoQuad->BufferData(GL_BufferHandler::ArrayBuffer, sizeof(verts), verts, GL_BufferHandler::StaticDraw);

    glEnableVertexAttribArray(GL_ShaderProgram::ShaderAttribs::VertexPos);
    glVertexAttribPointer(GL_ShaderProgram::ShaderAttribs::VertexPos, 3, GL_FLOAT, GL_FALSE, sizeof(Vector), 0);

    GL_VertexArrayObject::ResetVAOBinding();
}

void CVideoEngine::DrawVideo(float flTime)
{
    if (!m_pVideoTexture)
        return;

    if (videoended)
    {
        delete m_pVideoTexture;
        m_pVideoTexture = nullptr;
        ClearVideo();
        return;
    }

    double currentvideotime = engine_cl->time - start_time;
    if (!start_time)
        start_time = currentvideotime;

    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_2D, m_pVideoTexture->GetTextureID());

    if (currentvideotime >= viddata.pts) {

        videoended = !video_reader_readframe(&viddata, frame_data);
        if (videoended)
            return;

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viddata.width, viddata.height, GL_RGBA, GL_UNSIGNED_BYTE, frame_data);
    }

    drawFrame(&viddata);

}