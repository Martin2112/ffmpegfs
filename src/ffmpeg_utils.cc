/*
 * Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * On Debian systems, the complete text of the GNU General Public License
 * Version 3 can be found in `/usr/share/common-licenses/GPL-3'.
 */

/**
 * @file
 * @brief FFmpegfs utility set implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2021 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "ffmpeg_utils.h"
#include "id3v1tag.h"
#include "ffmpegfs.h"

#include <iostream>
#include <libgen.h>
#include <unistd.h>
#include <algorithm>
#include <wordexp.h>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif
// Disable annoying warnings outside our code
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#include <libswscale/swscale.h>
#if LAVR_DEPRECATE
#include <libswresample/swresample.h>
#else
#include <libavresample/avresample.h>
#endif
#include "libavutil/ffversion.h"
#pragma GCC diagnostic pop
#ifdef __cplusplus
}
#endif

static int is_device(__attribute__((unused)) const AVClass *avclass);
static std::string ffmpeg_libinfo(bool lib_exists, __attribute__((unused)) unsigned int version, __attribute__((unused)) const char *cfg, int version_minor, int version_major, int version_micro, const char * libname);

#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 128                    /**< @brief Max. length of a FFmpeg error string */
#endif // AV_ERROR_MAX_STRING_SIZE

FFmpegfs_Format::FFmpegfs_Format()
    : m_format_name("")
    , m_fileext("")
    , m_filetype(FILETYPE_UNKNOWN)
    , m_video_codec_id(AV_CODEC_ID_NONE)
    , m_audio_codec_id(AV_CODEC_ID_NONE)
{

}

FFmpegfs_Format::FFmpegfs_Format(const std::string & format_name, const std::string & fileext, FILETYPE filetype, AVCodecID video_codec_id, AVCodecID audio_codec_id)
    : m_format_name(format_name)
    , m_fileext(fileext)
    , m_filetype(filetype)
    , m_video_codec_id(video_codec_id)
    , m_audio_codec_id(audio_codec_id)
{

}

bool FFmpegfs_Format::init(const std::string & desttype)
{
    int found = true;

    m_filetype              = get_filetype(desttype);

    // Please note that m_format_name is used to select the FFmpeg container
    // by passing it to avformat_alloc_output_context2().
    m_desttype              = desttype;
    switch (m_filetype)
    {
    case FILETYPE_MP3:
    {
        m_audio_codec_id    = AV_CODEC_ID_MP3;
        m_video_codec_id    = AV_CODEC_ID_NONE;
        m_format_name       = "mp3";
        m_fileext           = "mp3";
        break;
    }
    case FILETYPE_MP4:
    {
        m_audio_codec_id    = AV_CODEC_ID_AAC;
        m_video_codec_id    = AV_CODEC_ID_H264;
        m_format_name       = "mp4";
        m_fileext           = "mp4";
        break;
    }
    case FILETYPE_WAV:
    {
        m_audio_codec_id    = AV_CODEC_ID_PCM_S16LE;
        m_video_codec_id    = AV_CODEC_ID_NONE;
        m_format_name       = "wav";
        m_fileext           = "wav";
        break;
    }
    case FILETYPE_OGG:
    {
        m_audio_codec_id    = AV_CODEC_ID_VORBIS;
        m_video_codec_id    = AV_CODEC_ID_THEORA;
        m_format_name       = "ogg";
        m_fileext           = "ogg";
        break;
    }
    case FILETYPE_WEBM:
    {
        m_audio_codec_id    = AV_CODEC_ID_OPUS;
        m_video_codec_id    = AV_CODEC_ID_VP9;
        m_format_name       = "webm";
        m_fileext           = "webm";
        break;
    }
    case FILETYPE_MOV:
    {
        m_audio_codec_id    = AV_CODEC_ID_AAC;
        m_video_codec_id    = AV_CODEC_ID_H264;
        m_format_name       = "mov";
        m_fileext           = "mov";
        break;
    }
    case FILETYPE_AIFF:
    {
        m_audio_codec_id    = AV_CODEC_ID_PCM_S16BE;
        m_video_codec_id    = AV_CODEC_ID_NONE;
        m_format_name       = "aiff";
        m_fileext           = "aiff";
        break;
    }
    case FILETYPE_OPUS:
    {
        m_audio_codec_id    = AV_CODEC_ID_OPUS;
        m_video_codec_id    = AV_CODEC_ID_NONE;
        m_format_name       = "opus";
        m_fileext           = "opus";
        break;
    }
    case FILETYPE_PRORES:
    {
        m_audio_codec_id    = AV_CODEC_ID_PCM_S16LE;
        m_video_codec_id    = AV_CODEC_ID_PRORES;
        m_format_name       = "mov";
        m_fileext           = "mov";
        break;
    }
    case FILETYPE_ALAC:
    {
        m_audio_codec_id    = AV_CODEC_ID_ALAC;
        m_video_codec_id    = AV_CODEC_ID_NONE;
        m_format_name       = "m4a";
        m_fileext           = "m4a";
        break;
    }
    case FILETYPE_PNG:
    {
        m_audio_codec_id    = AV_CODEC_ID_NONE;
        m_video_codec_id    = AV_CODEC_ID_PNG;
        m_format_name       = "png";
        m_fileext           = "png";
        break;
    }
    case FILETYPE_JPG:
    {
        m_audio_codec_id    = AV_CODEC_ID_NONE;
        m_video_codec_id    = AV_CODEC_ID_MJPEG;
        m_format_name       = "jpg";
        m_fileext           = "jpg";
        break;
    }
    case FILETYPE_BMP:
    {
        m_audio_codec_id    = AV_CODEC_ID_NONE;
        m_video_codec_id    = AV_CODEC_ID_BMP;
        m_format_name       = "bmp";
        m_fileext           = "bmp";
        break;
    }
    case FILETYPE_TS:
    case FILETYPE_HLS:
    {
        // AC3 possible in container, but not supported in browsers:
        // m_audio_codec_id    = AV_CODEC_ID_AC3;
        // Also allowed:
        // m_audio_codec_id    = AV_CODEC_ID_MP3;
        m_audio_codec_id    = AV_CODEC_ID_AAC;
        m_video_codec_id    = AV_CODEC_ID_H264;
        m_format_name       = "mpegts";
        m_fileext           = "ts";
        break;
    }
    case FILETYPE_UNKNOWN:
    {
        found = false;
        break;
    }
    }

    return found;
}

const std::string & FFmpegfs_Format::desttype() const
{
    return m_desttype;
}

const std::string & FFmpegfs_Format::format_name() const
{
    return m_format_name;
}

const std::string & FFmpegfs_Format::fileext() const
{
    return m_fileext;
}

FILETYPE FFmpegfs_Format::filetype() const
{
    return m_filetype;
}

bool FFmpegfs_Format::is_multiformat() const
{
    return  (is_frameset() || is_hls());
}

bool FFmpegfs_Format::is_frameset() const
{
    return (m_filetype == FILETYPE_JPG || m_filetype == FILETYPE_PNG || m_filetype == FILETYPE_BMP);
}

bool FFmpegfs_Format::is_hls() const
{
    return (m_filetype == FILETYPE_HLS);
}

AVCodecID FFmpegfs_Format::video_codec_id() const
{
    return m_video_codec_id;
}

AVCodecID FFmpegfs_Format::audio_codec_id() const
{
    return m_audio_codec_id;
}

const std::string & append_sep(std::string * path)
{
    if (path->back() != '/')
    {
        *path += '/';
    }

    return *path;
}

const std::string & append_filename(std::string * path, const std::string & filename)
{
    append_sep(path);

    *path += filename;

    return *path;
}

const std::string & remove_sep(std::string * path)
{
    if (path->back() == '/')
    {
        (*path).pop_back();
    }

    return *path;
}

const std::string & remove_filename(std::string * filepath)
{
    char *p = new_strdup(*filepath);

    if (p == nullptr)
    {
        errno = ENOMEM;
        return *filepath;
    }

    *filepath = dirname(p);
    delete [] p;
    append_sep(filepath);
    return *filepath;
}

const std::string & remove_path(std::string *filepath)
{
    char *p = new_strdup(*filepath);

    if (p == nullptr)
    {
        errno = ENOMEM;
        return *filepath;
    }

    *filepath = basename(p);
    delete [] p;
    return *filepath;
}

const std::string & remove_ext(std::string *filepath)
{
    size_t found;

    found = filepath->rfind('.');

    if (found != std::string::npos)
    {
        // Have extension
        *filepath = filepath->substr(0, found);
    }
    return *filepath;
}

bool find_ext(std::string * ext, const std::string & filename)
{
    size_t found;

    found = filename.rfind('.');

    if (found == std::string::npos)
    {
        // No extension
        ext->clear();
        return false;
    }
    else
    {
        // Have extension
        *ext = filename.substr(found + 1);
        return true;
    }
}

const std::string & replace_ext(std::string * filepath, const std::string & ext)
{
    size_t found;

    found = filepath->rfind('.');

    if (found == std::string::npos)
    {
        // No extension, just add
        *filepath += '.';
    }
    else
    {
        // Have extension, so replace
        *filepath = filepath->substr(0, found + 1);
    }

    *filepath += ext;

    return *filepath;
}

const std::string & append_ext(std::string * filepath, const std::string & ext)
{
    size_t found;

    found = filepath->rfind('.');

    if (found == std::string::npos || strcasecmp(filepath->substr(found + 1), ext))
    {
        // No extension or different extension
        *filepath += '.' + ext;
    }

    return *filepath;
}

char * new_strdup(const std::string & str)
{
    size_t n = str.size() + 1;
    char * p = new(std::nothrow) char[n];

    if (p == nullptr)
    {
        errno = ENOMEM;
        return nullptr;
    }

    strncpy(p, str.c_str(), n);
    return p;
}

const std::string & get_destname(std::string *destfilepath, const std::string & filepath)
{
    *destfilepath = filepath;
    remove_path(destfilepath);
    replace_ext(destfilepath, params.current_format(filepath)->fileext());
    *destfilepath = params.m_mountpath + *destfilepath;

    return *destfilepath;
}

std::string ffmpeg_geterror(int errnum)
{
    if (errnum < 0)
    {
        char error[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(errnum, error, AV_ERROR_MAX_STRING_SIZE);
        return error;
    }
    else
    {
        return strerror(errnum);
    }
}

int64_t ffmpeg_rescale(int64_t ts, const AVRational & time_base)
{
    if (ts == AV_NOPTS_VALUE)
    {
        return AV_NOPTS_VALUE;
    }

    if (ts == 0)
    {
        return 0;
    }

    return av_rescale_q(ts, av_get_time_base_q(), time_base);
}

#if !HAVE_MEDIA_TYPE_STRING
const char *get_media_type_string(enum AVMediaType media_type)
{
    switch (media_type)
    {
    case AVMEDIA_TYPE_VIDEO:
        return "video";
    case AVMEDIA_TYPE_AUDIO:
        return "audio";
    case AVMEDIA_TYPE_DATA:
        return "data";
    case AVMEDIA_TYPE_SUBTITLE:
        return "subtitle";
    case AVMEDIA_TYPE_ATTACHMENT:
        return "attachment";
    default:
        return "unknown";
    }
}
#endif

/**
 * @brief Get FFmpeg library info.
 * @param[in] lib_exists - Set to true if library exists.
 * @param[in] version - Library version number.
 * @param[in] cfg - Library configuration.
 * @param[in] version_minor - Library version minor.
 * @param[in] version_major - Library version major.
 * @param[in] version_micro - Library version micro.
 * @param[in] libname - Name of the library.
 * @return Formatted library information.
 */
static std::string ffmpeg_libinfo(bool lib_exists, __attribute__((unused)) unsigned int version, __attribute__((unused)) const char *cfg, int version_minor, int version_major, int version_micro, const char * libname)
{
    std::string info;

    if (lib_exists)
    {
        string_format(info,
                      "lib%-17s: %d.%d.%d\n",
                      libname,
                      version_minor,
                      version_major,
                      version_micro);
    }

    return info;
}

#define PRINT_LIB_INFO(libname, LIBNAME) \
    ffmpeg_libinfo(true, libname##_version(), libname##_configuration(), \
    LIB##LIBNAME##_VERSION_MAJOR, LIB##LIBNAME##_VERSION_MINOR, LIB##LIBNAME##_VERSION_MICRO, #libname)     /**< @brief Print info about a FFmpeg library */

std::string ffmpeg_libinfo()
{
    std::string info;

    info = "FFmpeg Version      : " FFMPEG_VERSION "\n";

    info += PRINT_LIB_INFO(avutil,      AVUTIL);
    info += PRINT_LIB_INFO(avcodec,     AVCODEC);
    info += PRINT_LIB_INFO(avformat,    AVFORMAT);
    // info += PRINT_LIB_INFO(avdevice,    AVDEVICE);
    // info += PRINT_LIB_INFO(avfilter,    AVFILTER);
#if LAVR_DEPRECATE
    info += PRINT_LIB_INFO(swresample,  SWRESAMPLE);
#else
    info += PRINT_LIB_INFO(avresample,  AVRESAMPLE);
#endif
    info += PRINT_LIB_INFO(swscale,     SWSCALE);
    // info += PRINT_LIB_INFO(postproc,    POSTPROC);

    return info;
}

/**
 * @brief Check if class is a FMmpeg device
 * @todo Currently always returns 0. Must implement real check.
 * @param[in] avclass - Private class object
 * @return Returns 1 if object is a device, 0 if not.
 */
static int is_device(__attribute__((unused)) const AVClass *avclass)
{
    //if (avclass == nullptr)
    //    return 0;

    return 0;
    //return AV_IS_INPUT_DEVICE(avclass->category) || AV_IS_OUTPUT_DEVICE(avclass->category);
}

int show_caps(int device_only)
{
    const AVInputFormat *ifmt  = nullptr;
    const AVOutputFormat *ofmt = nullptr;
    const char *last_name;
    int is_dev;

    std::printf("%s\n"
                " D. = Demuxing supported\n"
                " .E = Muxing supported\n"
                " --\n", device_only ? "Devices:" : "File formats:");
    last_name = "000";
    for (;;)
    {
        int decode = 0;
        int encode = 0;
        const char *name      = nullptr;
        const char *long_name = nullptr;
        const char *extensions = nullptr;

#if LAVF_DEP_AV_REGISTER
        void *ofmt_opaque = nullptr;
        ofmt_opaque = nullptr;
        while ((ofmt = av_muxer_iterate(&ofmt_opaque)))
#else
        while ((ofmt = av_oformat_next(ofmt)))
#endif
        {
            is_dev = is_device(ofmt->priv_class);
            if (!is_dev && device_only)
            {
                continue;
            }

            if ((!name || strcmp(ofmt->name, name) < 0) &&
                    strcmp(ofmt->name, last_name) > 0)
            {
                name        = ofmt->name;
                long_name   = ofmt->long_name;
                encode      = 1;
            }
        }
#if LAVF_DEP_AV_REGISTER
        void *ifmt_opaque = nullptr;
        ifmt_opaque = nullptr;
        while ((ifmt = av_demuxer_iterate(&ifmt_opaque)))
#else
        while ((ifmt = av_iformat_next(ifmt)))
#endif
        {
            is_dev = is_device(ifmt->priv_class);
            if (!is_dev && device_only)
            {
                continue;
            }

            if ((!name || strcmp(ifmt->name, name) < 0) &&
                    strcmp(ifmt->name, last_name) > 0)
            {
                name        = ifmt->name;
                long_name   = ifmt->long_name;
                extensions  = ifmt->extensions;
                encode      = 0;
            }
            if (name && strcmp(ifmt->name, name) == 0)
            {
                decode      = 1;
            }
        }

        if (!name)
        {
            break;
        }

        last_name = name;
        if (!extensions)
        {
            continue;
        }

        std::printf(" %s%s %-15s %-15s %s\n",
                    decode ? "D" : " ",
                    encode ? "E" : " ",
                    extensions != nullptr ? extensions : "-",
                    name,
                    long_name ? long_name:" ");
    }
    return 0;
}

const char * get_codec_name(AVCodecID codec_id, bool long_name)
{
    const AVCodecDescriptor * pCodecDescriptor;
    const char * psz = "unknown";

    pCodecDescriptor = avcodec_descriptor_get(codec_id);

    if (pCodecDescriptor != nullptr)
    {
        if (pCodecDescriptor->long_name != nullptr && long_name)
        {
            psz = pCodecDescriptor->long_name;
        }

        else
        {
            psz = pCodecDescriptor->name;
        }
    }

    return psz;
}

int mktree(const std::string & path, mode_t mode)
{
    char *buffer = new_strdup(path);

    if (buffer == nullptr)
    {
        return ENOMEM;
    }

    std::string dir;
    char *p = strtok (buffer, "/");
    int status = 0;

    while (p != nullptr)
    {
        int newstat;

        dir += "/";
        dir += p;

        errno = 0;

        newstat = mkdir(dir.c_str(), mode);

        if (!status && newstat && errno != EEXIST)
        {
            status = -1;
            break;
        }

        status = newstat;

        p = strtok (nullptr, "/");
    }

    delete [] buffer;

    return status;
}

void tempdir(std::string & path)
{
    const char *temp = getenv("TMPDIR");

    if (temp != nullptr)
    {
        path = temp;
        return;
    }

    path = P_tmpdir;

    if (!path.empty())
    {
        return;
    }

    path = "/tmp";
}

int supports_albumart(FILETYPE filetype)
{
    // Could also allow OGG but the format requires special handling for album arts
    return (filetype == FILETYPE_MP3 || filetype == FILETYPE_MP4);
}

FILETYPE get_filetype(const std::string & desttype)
{
    const std::map<std::string, FILETYPE, comp> m_filetype_map =
    {
        { "mp3",    FILETYPE_MP3 },
        { "mp4",    FILETYPE_MP4 },
        { "wav",    FILETYPE_WAV },
        { "ogg",    FILETYPE_OGG },
        { "webm",   FILETYPE_WEBM },
        { "mov",    FILETYPE_MOV },
        { "aiff",   FILETYPE_AIFF },
        { "opus",   FILETYPE_OPUS },
        { "prores", FILETYPE_PRORES },
        { "alac",   FILETYPE_ALAC },
        { "png",    FILETYPE_PNG },
        { "jpg",    FILETYPE_JPG },
        { "bmp",    FILETYPE_BMP },
        { "ts",     FILETYPE_TS },
        { "hls",    FILETYPE_HLS },
    };

    try
    {
        return (m_filetype_map.at(desttype));
    }
    catch (const std::out_of_range& /*oor*/)
    {
        //std::cerr << "Out of Range error: " << oor.what() << std::endl;
        return FILETYPE_UNKNOWN;
    }
}

FILETYPE get_filetype_from_list(const std::string & desttypelist)
{
    std::vector<std::string> desttype = split(desttypelist, ",");
    FILETYPE filetype = FILETYPE_UNKNOWN;

    // Find first matching entry
    for (size_t n = 0; n < desttype.size() && filetype != FILETYPE_UNKNOWN; n++)
    {
        filetype = get_filetype(desttype[n]);
    }
    return filetype;
}

void init_id3v1(ID3v1 *id3v1)
{
    // Initialise ID3v1.1 tag structure
    memset(id3v1, ' ', sizeof(ID3v1));
    memcpy(&id3v1->m_tag, "TAG", 3);
    id3v1->m_padding = '\0';
    id3v1->m_title_no = 0;
    id3v1->m_genre = 0;
}

std::string format_number(int64_t value)
{
    if (!value)
    {
        return "unlimited";
    }

    if (value == AV_NOPTS_VALUE)
    {
        return "unset";
    }

    return string_format("%" PRId64, value);
}

std::string format_bitrate(BITRATE value)
{
    if (value == static_cast<BITRATE>(AV_NOPTS_VALUE))
    {
        return "unset";
    }

    if (value > 1000000)
    {
        return string_format("%.2f Mbps", static_cast<double>(value) / 1000000);
    }
    else if (value > 1000)
    {
        return string_format("%.1f kbps", static_cast<double>(value) / 1000);
    }
    else
    {
        return string_format("%" PRId64 " bps", value);
    }
}

std::string format_samplerate(int value)
{
    if (value == static_cast<int>(AV_NOPTS_VALUE))
    {
        return "unset";
    }

    if (value < 1000)
    {
        return string_format("%u Hz", value);
    }
    else
    {
        return string_format("%.3f kHz", static_cast<double>(value) / 1000);
    }
}

#define STR_VALUE(arg)  #arg                                /**< @brief Convert macro to string */
#define X(name)         STR_VALUE(name)                     /**< @brief Convert macro to string */

std::string format_duration(int64_t value, uint32_t fracs /*= 3*/)
{
    if (value == AV_NOPTS_VALUE)
    {
        return "unset";
    }

    std::string buffer;
    unsigned hours   = static_cast<unsigned>((value / AV_TIME_BASE) / (3600));
    unsigned mins    = static_cast<unsigned>(((value / AV_TIME_BASE) % 3600) / 60);
    unsigned secs    = static_cast<unsigned>((value / AV_TIME_BASE) % 60);

    if (hours)
    {
        buffer = string_format("%02u:", hours);
    }

    buffer += string_format("%02u:%02u", mins, secs);
    if (fracs)
    {
        unsigned decimals    = static_cast<unsigned>(value % AV_TIME_BASE);
        buffer += string_format(".%0*u", sizeof(X(AV_TIME_BASE)) - 2, decimals).substr(0, fracs + 1);
    }
    return buffer;
}

std::string format_time(time_t value)
{
    if (!value)
    {
        return "unlimited";
    }

    if (value == static_cast<time_t>(AV_NOPTS_VALUE))
    {
        return "unset";
    }

    std::string buffer;
    int weeks;
    int days;
    int hours;
    int mins;
    int secs;

    weeks = static_cast<int>(value / (60*60*24*7));
    value -= weeks * (60*60*24*7);
    days = static_cast<int>(value / (60*60*24));
    value -= days * (60*60*24);
    hours = static_cast<int>(value / (60*60));
    value -= hours * (60*60);
    mins = static_cast<int>(value / (60));
    value -= mins * (60);
    secs = static_cast<int>(value);

    if (weeks)
    {
        buffer = string_format("%iw ", weeks);
    }
    if (days)
    {
        buffer += string_format("%id ", days);
    }
    if (hours)
    {
        buffer += string_format("%ih ", hours);
    }
    if (mins)
    {
        buffer += string_format("%im ", mins);
    }
    if (secs)
    {
        buffer += string_format("%is ", secs);
    }
    return buffer;
}

std::string format_size(uint64_t value)
{
    if (!value)
    {
        return "unlimited";
    }

    if (value == static_cast<uint64_t>(AV_NOPTS_VALUE))
    {
        return "unset";
    }

    if (value > 1024*1024*1024*1024LL)
    {
        return string_format("%.3f TB", static_cast<double>(value) / (1024*1024*1024*1024LL));
    }
    else if (value > 1024*1024*1024)
    {
        return string_format("%.2f GB", static_cast<double>(value) / (1024*1024*1024));
    }
    else if (value > 1024*1024)
    {
        return string_format("%.1f MB", static_cast<double>(value) / (1024*1024));
    }
    else if (value > 1024)
    {
        return string_format("%.1f KB", static_cast<double>(value) / (1024));
    }
    else
    {
        return string_format("%" PRIu64 " bytes", value);
    }
}

std::string format_size_ex(uint64_t value)
{
    return format_size(value) + string_format(" (%" PRIu64 " bytes)", value);
}

std::string format_result_size(size_t size_resulting, size_t size_predicted)
{
    if (size_resulting >= size_predicted)
    {
        size_t value = size_resulting - size_predicted;
        return format_size(value);
    }
    else
    {
        size_t value = size_predicted - size_resulting;
        return "-" + format_size(value);
    }
}

std::string format_result_size_ex(size_t size_resulting, size_t size_predicted)
{
    if (size_resulting >= size_predicted)
    {
        size_t value = size_resulting - size_predicted;
        return format_size(value) + string_format(" (%zu bytes)", value);
    }
    else
    {
        size_t value = size_predicted - size_resulting;
        return "-" + format_size(value) + string_format(" (-%zu bytes)", value);
    }
}

/**
 * @brief Print frames per second.
 * @param[in] d - Frames per second.
 * @param[in] postfix - Postfix text.
 */
static void print_fps(double d, const char *postfix)
{
    long v = lrint(d * 100);
    if (!v)
    {
        std::printf("%1.4f %s\n", d, postfix);
    }
    else if (v % 100)
    {
        std::printf("%3.2f %s\n", d, postfix);
    }
    else if (v % (100 * 1000))
    {
        std::printf("%1.0f %s\n", d, postfix);
    }
    else
    {
        std::printf("%1.0fk %s\n", d / 1000, postfix);
    }
}

int print_stream_info(const AVStream* stream)
{
    int ret = 0;

#if LAVF_DEP_AVSTREAM_CODEC
    AVCodecContext *avctx = avcodec_alloc_context3(nullptr);
    if (avctx == nullptr)
    {
        return AVERROR(ENOMEM);
    }

    ret = avcodec_parameters_to_context(avctx, stream->codecpar);
    if (ret < 0)
    {
        avcodec_free_context(&avctx);
        return ret;
    }

    // Fields which are missing from AVCodecParameters need to be taken from the AVCodecContext
    //            avctx->properties   = output_stream->codec->properties;
    //            avctx->codec        = output_stream->codec->codec;
    //            avctx->qmin         = output_stream->codec->qmin;
    //            avctx->qmax         = output_stream->codec->qmax;
    //            avctx->coded_width  = output_stream->codec->coded_width;
    //            avctx->coded_height = output_stream->codec->coded_height;
#else
    AVCodecContext *avctx = stream->codec;
#endif
    int fps = stream->avg_frame_rate.den && stream->avg_frame_rate.num;
    int tbr = stream->r_frame_rate.den && stream->r_frame_rate.num;
    int tbn = stream->time_base.den && stream->time_base.num;
    int tbc = avctx->time_base.den && avctx->time_base.num; // Even the currently latest (lavf 58.10.100) refers to AVStream codec->time_base member... (See dump.c dump_stream_format)

    if (fps)
        print_fps(av_q2d(stream->avg_frame_rate), "avg fps");
    if (tbr)
        print_fps(av_q2d(stream->r_frame_rate), "Real base framerate (tbr)");
    if (tbn)
        print_fps(1 / av_q2d(stream->time_base), "stream timebase (tbn)");
    if (tbc)
        print_fps(1 / av_q2d(avctx->time_base), "codec timebase (tbc)");

#if LAVF_DEP_AVSTREAM_CODEC
    avcodec_free_context(&avctx);
#endif

    return ret;
}

void exepath(std::string * path)
{
    char result[PATH_MAX + 1] = "";
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count != -1)
    {
        *path = dirname(result);
        append_sep(path);
    }
    else
    {
        path->clear();
    }
}

std::string &ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

std::string replace_all(std::string str, const std::string& from, const std::string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

int strcasecmp(const std::string & s1, const std::string & s2)
{
    return ::strcasecmp(s1.c_str(), s2.c_str());
}

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
    size_t size = static_cast<size_t>(snprintf(nullptr, 0, format.c_str(), args ...) + 1); // Extra space for '\0'
    std::unique_ptr<char[]> buf(new(std::nothrow) char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

int reg_compare(const std::string & value, const std::string & pattern, std::regex::flag_type flag)
{
    int reti;

    try
    {
        std::regex rgx(pattern, flag);

        reti = (std::regex_search(value, rgx) == true) ? 0 : 1;
    }
    catch(const std::regex_error& e)
    {
        std::cerr << "regex_error caught: " << e.what() << '\n';
        if(e.code() == std::regex_constants::error_brack)
            std::cerr << "The code was error_brack\n";

        reti = -1;
    }

    return reti;
}

const std::string & expand_path(std::string *tgt, const std::string & src)
{
    wordexp_t exp_result;
    if (!wordexp(replace_all(src, " ", "\\ ").c_str(), &exp_result, 0))
    {
        *tgt = exp_result.we_wordv[0];
        wordfree(&exp_result);
    }
    else
    {
        *tgt =  src;
    }

    return *tgt;
}

int is_mount(const std::string & path)
{
    char* orig_name = nullptr;
    int ret = 0;

    try
    {
        struct stat file_stat;
        struct stat parent_stat;
        char * parent_name = nullptr;

        orig_name = new_strdup(path);

        if (orig_name == nullptr)
        {
            std::fprintf(stderr, "is_mount(): Out of memory\n");
            errno = ENOMEM;
            throw -1;
        }

        // get the parent directory of the file
        parent_name = dirname(orig_name);

        // get the file's stat info
        if (-1 == stat(path.c_str(), &file_stat))
        {
            std::fprintf(stderr, "is_mount(): (%i) %s\n", errno, strerror(errno));
            throw -1;
        }

        //determine whether the supplied file is a directory
        // if it isn't, then it can't be a mountpoint.
        if (!(file_stat.st_mode & S_IFDIR))
        {
            std::fprintf(stderr, "is_mount(): %s is not a directory.\n", path.c_str());
            throw -1;
        }

        // get the parent's stat info
        if (-1 == stat(parent_name, &parent_stat))
        {
            std::fprintf(stderr, "is_mount(): (%i) %s\n", errno, strerror(errno));
            throw -1;
        }

        // if file and parent have different device ids,
        // then the file is a mount point
        // or, if they refer to the same file,
        // then it's probably the root directory
        // and therefore a mountpoint
        if (file_stat.st_dev != parent_stat.st_dev ||
                (file_stat.st_dev == parent_stat.st_dev &&
                 file_stat.st_ino == parent_stat.st_ino))
        {
            // IS a mountpoint
            ret = 1;
        }
        else
        {
            // is NOT a mountpoint
            ret = 0;
        }
    }
    catch (int _ret)
    {
        ret = _ret;
    }

    delete [] orig_name;

    return ret;
}

std::vector<std::string> split(const std::string& input, const std::string & regex)
{
    // passing -1 as the submatch index parameter performs splitting
    std::regex re(regex);
    std::sregex_token_iterator first{input.cbegin(), input.cend(), re, -1},
    last;
    return {first, last};
}

std::string sanitise_filepath(std::string * filepath)
{
    char resolved_name[PATH_MAX + 1];

    if (realpath(filepath->c_str(), resolved_name) != nullptr)
    {
        *filepath = resolved_name;
        return *filepath;
    }

    // realpath has the strange feature to remove a traling slash if there.
    // To mimick its behaviour, if realpath fails, at least remove it.
    std::string _filepath(*filepath);
    remove_sep(&_filepath);
    return _filepath;
}

std::string sanitise_filepath(const std::string & filepath)
{
    std::string buffer(filepath);
    return sanitise_filepath(&buffer);
}

bool is_album_art(AVCodecID codec_id, const AVRational * frame_rate)
{
    if (codec_id == AV_CODEC_ID_PNG || codec_id == AV_CODEC_ID_BMP)
    {
        // PNG or BMP: must be an album art stream
        return true;
    }

    if (codec_id != AV_CODEC_ID_MJPEG)
    {
        // Anything else than MJPEG is never an album art stream
        return false;
    }

    if (frame_rate != nullptr && frame_rate->den)
    {
        double dbFrameRate = static_cast<double>(frame_rate->num) / frame_rate->den;

        // If frame rate is < 100 fps this most likely is a video
        if (dbFrameRate >= 100)
        {
            // Not a video
            return false;
        }
    }

    return false;
}

bool nocasecompare(const std::string & lhs, const std::string &rhs)
{
    return (strcasecmp(lhs, rhs) < 0);
}

size_t get_disk_free(std::string & path)
{
    struct statvfs buf;

    if (statvfs(path.c_str(), &buf))
    {
        return 0;
    }

    return static_cast<size_t>(buf.f_bfree * buf.f_bsize);
}

bool check_ignore(size_t size, size_t offset)
{
    size_t blocksize[] = { 0x2000, 0x8000, 0x10000, 0 };
    bool ignore = false;

    for (int n = 0; blocksize[n] && !ignore; n++)
    {
        size_t rest;
        bool match;

        match = !(offset % blocksize[n]);           // Must be multiple of block size
        if (!match)
        {
            continue;
        }

        rest = size % offset;                       // Calculate rest. Cast OK, offset can never be < 0.
        ignore = match && (rest < blocksize[n]);    // Ignore of rest is less than block size
    }

    return ignore;
}

std::string make_filename(uint32_t file_no, const std::string & fileext)
{
    return string_format("%06u.%s", file_no, fileext.c_str());
}

bool file_exists(const std::string & filename)
{
    return (access(filename.c_str(), F_OK) != -1);
}

void make_upper(std::string * input)
{
    std::for_each(std::begin(*input), std::end(*input), [](char& c) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });
}

void make_lower(std::string * input)
{
    std::for_each(std::begin(*input), std::end(*input), [](char& c) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
}
