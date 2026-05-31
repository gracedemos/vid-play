#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static SDL_AudioStream* sdl_audio = NULL;

static int init_sdl(int width, int height)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        return -1;

    if (!SDL_CreateWindowAndRenderer("Vid Play", width, height, SDL_WINDOW_RESIZABLE, &window, &renderer))
        return -1;

    if (!SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX))
        return -1;

    if ((texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, width, height)) == NULL)
        return -1;

    SDL_AudioSpec spec = {};
    spec.channels = 2;
    spec.format = SDL_AUDIO_F32;
    spec.freq = 48000;
    if ((sdl_audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL)) == NULL)
        return -1;

    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Invalid argument count\n");
        return -1;
    }

    AVFormatContext* fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) < 0)
    {
        fprintf(stderr, "Error: Failed to open file\n");
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
    {
        fprintf(stderr, "Error: Failed to find stream info\n");
        return -1;
    }

    const AVCodec* video_codec = NULL;
    int video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    if (video_stream < 0)
    {
        fprintf(stderr, "Error: Video stream not found\n");
        return -1;
    }

    const AVCodec* audio_codec = NULL;
    int audio_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_codec, 0);
    if (audio_stream < 0)
    {
        fprintf(stderr, "Error: Audio stream not found\n");
        return -1;
    }

    printf("Container format: %s\n", fmt_ctx->iformat->name);
    printf("Stream count: %u\n", fmt_ctx->nb_streams);
    printf("\n[Stream %d] Video codec: %s\n", video_stream, video_codec->name);
    printf("\n[Stream %d] Audio codec: %s\n", audio_stream, audio_codec->name);

    AVCodecContext* video_codec_ctx = avcodec_alloc_context3(video_codec);
    AVStream* video = fmt_ctx->streams[video_stream];
    avcodec_parameters_to_context(video_codec_ctx, video->codecpar);

    AVCodecContext* audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    AVStream* audio = fmt_ctx->streams[audio_stream];
    avcodec_parameters_to_context(audio_codec_ctx, audio->codecpar);

    /*AVBufferRef* hw_device_ctx = NULL;
    if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, NULL, NULL, 0) < 0)
    {
        fprintf(stderr, "Error: Failed to create HW device\n");
        return -1;
    }
    video_codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);*/

    if (avcodec_open2(video_codec_ctx, video_codec, NULL) < 0)
    {
        fprintf(stderr, "Error: Failed to open video codec\n");
        return -1;
    }

    if (avcodec_open2(audio_codec_ctx, audio_codec, NULL) < 0)
    {
        fprintf(stderr, "Error: Failed to open audio codec\n");
        return -1;
    }

    if (init_sdl(video_codec_ctx->width, video_codec_ctx->height) < 0)
    {
        fprintf(stderr, "Error: Failed to init SDL\n");
        return -1;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int quit = 0;
    SDL_ResumeAudioStreamDevice(sdl_audio);
    while (!quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
                quit = 1;
        }

        if (av_read_frame(fmt_ctx, packet) == 0)
        {
            if (packet->stream_index == video_stream)
            {
                if (avcodec_send_packet(video_codec_ctx, packet) == 0)
                {
                    while (avcodec_receive_frame(video_codec_ctx, frame) == 0)
                    {
                        SDL_UpdateYUVTexture(texture, NULL,
                            frame->data[0],
                            frame->linesize[0],
                            frame->data[1],
                            frame->linesize[1],
                            frame->data[2],
                            frame->linesize[2]);
                    }
                }
            }
            if (packet->stream_index == audio_stream)
            {
                if (avcodec_send_packet(audio_codec_ctx, packet) == 0)
                {
                    while (avcodec_receive_frame(audio_codec_ctx, frame) == 0)
                    {
                        int data_size = sizeof(float) * 2 * frame->nb_samples;
                        float* interleave_buf = av_malloc(data_size);
                        for (int i = 0; i < frame->nb_samples; i++)
                        {
                            interleave_buf[i * 2] = ((float*)frame->data[0])[i];
                            interleave_buf[i * 2 + 1] = ((float*)frame->data[1])[i];
                        }
                        SDL_PutAudioStreamData(sdl_audio, interleave_buf, data_size);
                        av_freep(&interleave_buf);
                    }
                }
            }
            av_packet_unref(packet);
        }

        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    return 0;
}
