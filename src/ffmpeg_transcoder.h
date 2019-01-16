/*
 * FFmpeg decoder class header for ffmpegfs
 *
 * Copyright (C) 2017-2019 Norbert Schlia (nschlia@oblivion-software.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef FFMPEG_TRANSCODER_H
#define FFMPEG_TRANSCODER_H

#pragma once

#include "ffmpeg_base.h"
#include "id3v1tag.h"
#include "ffmpegfs.h"
#include "fileio.h"

#include <queue>

class Buffer;
#if LAVR_DEPRECATE
struct SwrContext;
#else
struct AVAudioResampleContext;
#endif
struct SwsContext;
struct AVFilterContext;
struct AVFilterContext;
struct AVFilterGraph;
struct AVAudioFifo;

class FFMPEG_Transcoder : public FFMPEG_Base
{
public:
#define OPT_CODEC       0x80000000

#define OPT_ALL         0x00000000  // All files
#define OPT_AUDIO       0x00000001  // For audio only files
#define OPT_VIDEO       0x00000002  // For videos (not audio only)

    typedef struct _tagPROFILE_OPTION
    {
        const char *            m_key;
        const char *            m_value;
        const int               m_flags;
        const int               m_options;
    } PROFILE_OPTION, *LPPROFILE_OPTION;
    typedef PROFILE_OPTION const * LPCPROFILE_OPTION;

    typedef struct _tagPROFILE_LIST
    {
        FILETYPE                m_filetype;
        PROFILE                 m_profile;
        LPCPROFILE_OPTION       m_option_codec;
        LPCPROFILE_OPTION       m_option_format;
    } PROFILE_LIST, *LPPROFILE_LIST;
    typedef PROFILE_LIST const * LPCPROFILE_LIST;

#define MAX_PRORES_FRAMERATE    2

    // Predicted bitrates for Apple Prores, see https://www.apple.com/final-cut-pro/docs/Apple_ProRes_White_Paper.pdf
    typedef struct _tagPRORES_BITRATE
    {
        int                     m_width;
        int                     m_height;
        struct _tagPRORES_FRAMERATE
        {
            int                 m_framerate;
            int                 m_interleaved;
        }                       m_framerate[MAX_PRORES_FRAMERATE];
        // Bitrates in MB/s
        // 0: ProRes 422 Proxy
        // 1: ProRes 422 LT
        // 2: ProRes 422 standard
        // 3: ProRes 422 HQ
        // 4: ProRes 4444 (no alpha)
        // 5: ProRes 4444 XQ (no alpha)
        int                     m_bitrate[6];
    } PRORES_BITRATE, *LPPRORES_BITRATE;
    typedef PRORES_BITRATE const * LPCPRORES_BITRATE;

    struct STREAMREF
    {
        STREAMREF() :
            m_codec_ctx(nullptr),
            m_stream(nullptr),
            m_stream_idx(INVALID_STREAM)
        {}

        AVCodecContext *        m_codec_ctx;
        AVStream *              m_stream;
        int                     m_stream_idx;
    };

    // Input file
    struct INPUTFILE
    {
        INPUTFILE() :
            m_file_type(FILETYPE_UNKNOWN),
            m_filename("unset"),
            m_format_ctx(nullptr)
        {}

        FILETYPE                m_file_type;
        std::string             m_filename;

        AVFormatContext *       m_format_ctx;

        STREAMREF               m_audio;
        STREAMREF               m_video;

        std::vector<STREAMREF>  m_album_art;
    };

    // Output file
    struct OUTPUTFILE : public INPUTFILE
    {
        OUTPUTFILE() :
            m_nAudio_pts(0),
            m_video_start_pts(0),
            m_last_mux_dts(AV_NOPTS_VALUE)
        {}

        int64_t                 m_nAudio_pts;           // Global timestamp for the audio frames
        int64_t                 m_video_start_pts;      // Video start PTS
        int64_t                 m_last_mux_dts;         // Last muxed DTS

        ID3v1                   m_id3v1;                // mp3 only, can be referenced at any time
    };

public:
    FFMPEG_Transcoder();
    virtual ~FFMPEG_Transcoder();

    bool                        is_open() const;
    int                         open_input_file(LPVIRTUALFILE virtualfile, fileio * fio = nullptr);
    int                         open_output_file(Buffer* buffer);
    int                         process_single_fr(int & status);
    int                         encode_finish();
    void                        close();

    time_t                      mtime() const;
    size_t                      predicted_filesize();

    const ID3v1 *               id3v1tag() const;

    virtual const char *        filename() const;
    virtual const char *        destname() const;
    
    static bool                 audio_size(size_t *filesize, AVCodecID codec_id, BITRATE bit_rate, double duration, int channels, int sample_rate);
    static bool                 video_size(size_t *filesize, AVCodecID codec_id, BITRATE bit_rate, double duration, int width, int height, int interleaved, double frame_rate);

protected:
    bool                        is_video() const;
    int                         update_codec(void *opt, LPCPROFILE_OPTION mp4_opt) const;
    int                         prepare_codec(void *opt, FILETYPE filetype) const;
    int                         add_stream(AVCodecID codec_id);
    int                         add_stream_copy(AVCodecID codec_id, AVMediaType codec_type);
    int                         add_albumart_stream(const AVCodecContext *input_codec_ctx);
    int                         add_albumart_frame(AVStream *output_stream, AVPacket *pkt_in);
    int                         open_output_filestreams(Buffer *buffer);
    void                        copy_metadata(AVDictionary **metadata_out, const AVDictionary *metadata_in);
    int                         process_metadata();
    int                         process_albumarts();
    int                         init_resampler();
    int                         init_fifo();
    int                         update_format(AVDictionary** dict, LPCPROFILE_OPTION option) const;
    int                         prepare_format(AVDictionary **dict,  FILETYPE filetype) const;
    int                         write_output_file_header();
    int                         store_packet(AVPacket *pkt);
    int                         decode_audio_frame(AVPacket *pkt, int *decoded);
    int                         decode_video_frame(AVPacket *pkt, int *decoded);
    int                         decode_frame(AVPacket *pkt);
    int                         init_converted_samples(uint8_t ***converted_input_samples, int frame_size);
    int                         convert_samples(uint8_t **input_data, const int in_samples, uint8_t **converted_data, int *out_samples);
    int                         add_samples_to_fifo(uint8_t **converted_input_samples, const int frame_size);
    int                         flush_frames(int stream_index);
    int                         read_decode_convert_and_store(int *finished);
    int                         init_audio_output_frame(AVFrame **frame, int frame_size);
    AVFrame *                   alloc_picture(AVPixelFormat pix_fmt, int width, int height);
    void                        produce_audio_dts(AVPacket * pkt, int64_t *pts);
#if LAVC_NEW_PACKET_INTERFACE
    int                         decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt) const;
#endif
    int                         encode_audio_frame(AVFrame *frame, int *data_present);
    int                         encode_video_frame(AVFrame *frame, int *data_present);
    int                         load_encode_and_write(int frame_size);
    int                         write_output_file_trailer();

    static int                  input_read(void * opaque, unsigned char * data, int size);
    static int                  output_write(void * opaque, unsigned char * data, int size);
    static int64_t              seek(void * opaque, int64_t offset, int whence);

    static BITRATE              get_prores_bitrate(int width, int height, double framerate, int interleaved, int profile);
    size_t                      calculate_predicted_filesize() const;
    bool                        get_video_size(int *output_width, int *output_height) const;
    static bool                 get_output_sample_rate(int input_sample_rate, int max_sample_rate, int * output_sample_rate = nullptr);
    static bool                 get_output_bit_rate(BITRATE input_bit_rate, BITRATE max_bit_rate, BITRATE * output_bit_rate = nullptr);
    double                      get_aspect_ratio(int width, int height, const AVRational & sample_aspect_ratio) const;

#ifndef USING_LIBAV
    int                         init_filters(AVCodecContext *pCodecContext, AVStream *pStream);
    AVFrame *                   send_filters(AVFrame *srcframe, int &ret);
    void                        free_filters();
#endif // !USING_LIBAV

    bool                        can_copy_audio() const;
    bool                        can_copy_video() const;

    bool                        close_resample();

private:
    fileio *                    m_fileio;
    bool                        m_close_fileio;
    time_t                      m_mtime;
    size_t                      m_predicted_size;         // Use this as the size instead of computing it.
    bool                        m_is_video;

    // Audio conversion and buffering    
    AVSampleFormat              m_cur_sample_fmt;
    int                         m_cur_sample_rate;
    uint64_t                    m_cur_channel_layout;
#if LAVR_DEPRECATE
    SwrContext *                m_pAudio_resample_ctx;
#else
    AVAudioResampleContext *    m_pAudio_resample_ctx;
#endif
    AVAudioFifo *               m_pAudioFifo;

    // Video conversion and buffering
    SwsContext *                m_pSws_ctx;
    AVFilterContext *           m_pBufferSinkContext;
    AVFilterContext *           m_pBufferSourceContext;
    AVFilterGraph *             m_pFilterGraph;
    std::queue<AVFrame*>        m_VideoFifo;
    int64_t                     m_pts;
    int64_t                     m_pos;

    INPUTFILE                   m_in;
    OUTPUTFILE                  m_out;

    // If the audio and/or video stream is copied, packets will be stuffed into the packet queue.
    bool                        m_copy_audio;
    bool                        m_copy_video;

    ffmpegfs_format *           m_current_format;

    static const PROFILE_LIST   m_profile[];

    static const PRORES_BITRATE m_prores_bitrate[];
};

#endif // FFMPEG_TRANSCODER_H
