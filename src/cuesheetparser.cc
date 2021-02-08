﻿/*
 * Copyright (C) 2017-2020 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @brief Cue sheet parser implementation
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2020 Norbert Schlia (nschlia@oblivion-software.de)
 */

#include "ffmpegfs.h"
#include "cuesheetparser.h"
#include "transcode.h"
//#include "ffmpeg_utils.h"
#include "logging.h"

#include <libcue.h>

#define FPS (75)                                            ///<* On sector contains 75 frames per CD specs
#define VAL_OR_EMPTY(val)   ((val) != nullptr ? (val) : "") ///<* Return string or empty string if val is nullptr

#define TRACKDIR            "tracks"                        ///<* Extension of virtual tracks directory

/**
  * @brief Cuesheet structure
  * Structure see https://en.wikipedia.org/wiki/Cue_sheet_(computing) @n
  * @n
  * Real life example: @n
  * @n
  * PERFORMER "Subway to Sally" @n
  * TITLE "Nord Nord Ost" @n
  * CATALOG 0727361134129 @n
  * REM DATE 2005 @n
  * REM DISCNUMBER 1 @n
  * REM TOTALDISCS 1 @n
  * FILE "Subway to Sally - Nord Nord Ost.flac" WAVE @n
  *   TRACK 01 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Sarabande de noir" @n
  *     INDEX 01 00:00:00 @n
  *   TRACK 02 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Schneekönigin" @n
  *     INDEX 01 00:55:30 @n
  *   TRACK 03 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Feuerland" @n
  *     INDEX 01 06:41:38 @n
  *   TRACK 04 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Sieben" @n
  *     INDEX 01 10:48:42 @n
  *   TRACK 05 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Lacrimae '74" @n
  *     INDEX 01 14:11:14 @n
  *   TRACK 06 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Feuerkind" @n
  *     INDEX 01 15:57:07 @n
  *   TRACK 07 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Das Rätsel II" @n
  *     INDEX 01 22:03:28 @n
  *   TRACK 08 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "S.O.S." @n
  *     INDEX 01 26:25:23 @n
  *   TRACK 09 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Eisblumen" @n
  *     INDEX 01 32:21:42 @n
  *   TRACK 10 AUDIO @n
  *     PERFORMER "Subway to Sally" @n
  *     TITLE "Seemannslied" @n
  *     INDEX 01 36:53:71 @n
  */

//if (m_in.m_audio.m_stream != nullptr && CODECPAR(m_in.m_audio.m_stream)->codec_id == AV_CODEC_ID_VORBIS)
//{
//    // For some formats (namely ogg) FFmpeg returns the tags, odd enough, with streams...
//    copy_metadata(&m_out.m_format_ctx->metadata, m_in.m_audio.m_stream->metadata);
//}

//copy_metadata(&m_out.m_format_ctx->metadata, m_in.m_format_ctx->metadata);

//if (m_out.m_audio.m_stream != nullptr && m_in.m_audio.m_stream != nullptr)
//{
//    // Copy audio stream meta data
//    copy_metadata(&m_out.m_audio.m_stream->metadata, m_in.m_audio.m_stream->metadata);
//}

//if (m_out.m_video.m_stream != nullptr && m_in.m_video.m_stream != nullptr)
//{
//    // Copy video stream meta data
//    copy_metadata(&m_out.m_video.m_stream->metadata, m_in.m_video.m_stream->metadata);
//}

//// Also copy album art meta tags
//for (size_t n = 0; n < m_in.m_album_art.size(); n++)
//{
//    AVStream *input_stream = m_in.m_album_art.at(n).m_stream;
//    AVStream *output_stream = m_out.m_album_art.at(n).m_stream;

//    copy_metadata(&output_stream->metadata, input_stream->metadata);
//}

static bool create_cuesheet_virtualfile(Track *track, int titleno, const std::string &filename, const std::string & path, const struct stat * statbuf, int trackcount, int trackno, const std::string & album, const std::string &genre, const std::string &date);
//static int parse_cuesheet(const std::string & filename, const AVDictionary *metadata, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);
static int parse_cuesheet(const std::string & filename, const std::string & cuesheet, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);
static int parse_cuesheet(const std::string & filename, Cd *cd, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler);

/**
 * @brief Create a virtual file entry of a cue sheet title.
 * @param[in] track - libcue2 track handle
 * @param[in] titleno - Title number.
 * @param[in] filename - Source filename.
 * @param[in] path - Path to check.
 * @param[in] statbuf - File status structure of original file.
 * @param[in] trackcount - Number of tracks in cue sheet.
 * @param[in] trackno - Track number.
 * @param[in] album - Name of album.
 * @param[in] genre - Album genre.
 * @param[in] date - Publishing date.
 * @return On error, returns false. On success, returns true.
 */
static bool create_cuesheet_virtualfile(Track *track, int titleno, const std::string & filename, const std::string & path, const struct stat * statbuf, int trackcount, int trackno, const std::string & album, const std::string & genre, const std::string & date)
{
    Cdtext *cuesheetcdtext = track_get_cdtext(track);
    if (cuesheetcdtext == nullptr)
    {
        Logging::error(filename, "Unable to get track cd text from cue sheet.");
        errno = EIO;
        return false;
    }

    std::string performer   = VAL_OR_EMPTY(cdtext_get(PTI_PERFORMER, cuesheetcdtext));
    std::string title       = VAL_OR_EMPTY(cdtext_get(PTI_TITLE, cuesheetcdtext));

    int64_t start           = AV_TIME_BASE * static_cast<int64_t>(track_get_start(track)) / FPS;
    int64_t duration        = AV_TIME_BASE * static_cast<int64_t>(track_get_length(track)) / FPS;

    //for (int trackindexno = 1;; trackindexno++)
    //{
    //    long index = track_get_index(track, trackindexno);

    //    if (index == -1)
    //    {
    //        break;
    //    }

    //    int64_t trackindex = AV_TIME_BASE * static_cast<int64_t>(index) / FPS;

    ///<* @todo Evaluate track index here???
    //}

    char title_buf[PATH_MAX + 1];

    snprintf(title_buf, sizeof(title_buf) - 1, "%02d. %s - %s [%s].%s",
             titleno,
             performer.c_str(),
             title.c_str(),
             replace_all(format_duration(duration), ":", "-").c_str(),
             params.m_format[0].fileext().c_str());                     ///<* @todo Should use the correct index (audio) here

    std::string virtfilename(title_buf);

    LPVIRTUALFILE virtualfile = nullptr;
    if (!params.m_format[0].is_multiformat())
    {
        virtualfile = insert_file(VIRTUALTYPE_DISK, path + virtfilename, filename, statbuf, VIRTUALFLAG_CUESHEET);
    }
    else
    {
        virtualfile = insert_dir(VIRTUALTYPE_DISK, path + virtfilename, statbuf, VIRTUALFLAG_CUESHEET);
    }

    if (virtualfile == nullptr)
    {
        Logging::error(path, "Failed to create virtual path: %1", (path + virtfilename).c_str());
        errno = EIO;
        return false;
    }

    // We do not add the file to fuse here because it's in a sub directory.
    // Will be done later on request by load_path()

    virtualfile->m_format_idx           = 0;    ///<* @todo Should store the correct index (audio) in m_format_idx

    if (!transcoder_cached_filesize(virtualfile, &virtualfile->m_st))
    {
        BITRATE video_bit_rate  = 0;	///< @todo probe original file for info
        BITRATE audio_bit_rate  = 0;

        int channels            = 0;
        int sample_rate         = 0;
        //int audio               = 0;

        int width               = 0;
        int height              = 0;
        AVRational framerate    = { 0, 0 };
        int interleaved         = 0;

        virtualfile->m_duration                 = duration;
        virtualfile->m_cuesheet.m_duration      = duration;
        virtualfile->m_cuesheet.m_start         = start;
        virtualfile->m_cuesheet.m_tracktotal    = trackcount;
        virtualfile->m_cuesheet.m_trackno       = trackno;
        virtualfile->m_cuesheet.m_artist        = performer;
        virtualfile->m_cuesheet.m_title         = title;
        virtualfile->m_cuesheet.m_album         = album;
        virtualfile->m_cuesheet.m_genre         = genre;
        virtualfile->m_cuesheet.m_date          = date;

        transcoder_set_filesize(virtualfile, duration, audio_bit_rate, channels, sample_rate, video_bit_rate, width, height, interleaved, framerate);

        stat_set_size(&virtualfile->m_st, virtualfile->m_predicted_size);
    }

    return true;
}

#if 0
/**
 * @brief parse_cuesheet
 * @param[in] filename
 * @param[in] metadata
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of titles in cue sheet. On error, returns -errno.
 */
static int parse_cuesheet(const std::string & filename, const AVDictionary *metadata, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    AVDictionaryEntry *tag = nullptr;

    // Check for embedded cue sheet
    Logging::trace(filename, "Checking for embedded cue sheet.");

    tag = av_dict_get(metadata, "CUESHEET", tag, AV_DICT_IGNORE_SUFFIX);
    if (tag == nullptr || tag->value == nullptr)
    {
        // No embedded cue sheet
        return 0;
    }

    // Found embedded cue sheet
    Logging::trace(filename, "Found embedded cue sheet.");

    return parse_cuesheet(filename, cue_parse_string(tag->value), statbuf, buf, filler);
}
#endif

/**
 * @brief parse_cuesheet
 * @param[in] filename
 * @param[in] cuesheet
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of titles in cue sheet. On error, returns -errno.
 */
static int parse_cuesheet(const std::string & filename, const std::string & cuesheet, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    // Check for cue sheet
    std::string text;
    int res = read_file(cuesheet, text);
    if (res >= 0)
    {
        return -res;
    }

    // Found cue sheet
    Logging::trace(cuesheet, "Found cue sheet.");

    res = parse_cuesheet(filename, cue_parse_string(text.c_str()), statbuf, buf, filler);

    return res;
}

/**
 * @brief parse_cuesheet
 * @param[in] filename - File name of source
 * @param[in] cd - libcue2 cue sheet handle
 * @param[in] statbuf - File status structure of original file.
 * @param[in, out] buf - The buffer passed to the readdir() operation.
 * @param[in, out] filler - Function to add an entry in a readdir() operation (see https://libfuse.github.io/doxygen/fuse_8h.html#a7dd132de66a5cc2add2a4eff5d435660)
 * @return On success, returns number of titles in cue sheet. On error, returns -errno.
 */
static int parse_cuesheet(const std::string & filename, Cd *cd, const struct stat *statbuf, void *buf, fuse_fill_dir_t filler)
{
    int res = 0;

    try
    {
        if (cd == nullptr)
        {
            Logging::error(filename, "Unable to parse cue sheet.");
            throw AVERROR(EIO);
        }

        Rem *rem = cd_get_rem(cd);
        if (rem == nullptr)
        {
            Logging::error(filename, "Unable to parse remarks from cue sheet.");
            throw AVERROR(EIO);
        }

        Cdtext *cdtext = cd_get_cdtext(cd);
        if (cdtext == nullptr)
        {
            Logging::error(filename, "Unable to get cd text from cue sheet.");
            throw AVERROR(EIO);
        }

        ///<* @todo Probe source file here? -> sample rate, channels etc.

        std::string performer  = VAL_OR_EMPTY(cdtext_get(PTI_PERFORMER, cdtext));
        std::string album      = VAL_OR_EMPTY(cdtext_get(PTI_TITLE, cdtext));
        std::string genre      = VAL_OR_EMPTY(cdtext_get(PTI_GENRE, cdtext));
        std::string date       = VAL_OR_EMPTY(rem_get(REM_DATE, rem));

        int trackcount = static_cast<int>(cd_get_ntrack(cd));
        if (trackcount)
        {
            LPVIRTUALFILE virtualfile = nullptr;
            std::string subbdir(filename);

            append_ext(&subbdir, TRACKDIR);

            std::string dirname(subbdir);

            append_sep(&subbdir);
            remove_path(&dirname);

            virtualfile = insert_dir(VIRTUALTYPE_DISK, subbdir, statbuf, VIRTUALFLAG_CUESHEET);

            if (virtualfile == nullptr)
            {
                Logging::error(filename, "Failed to create virtual path: %1", subbdir.c_str());
                errno = EIO;
                return -errno;
            }

            if (buf != nullptr && filler(buf, dirname.c_str(), &virtualfile->m_st, 0))
            {
                // break;
            }

            std::string path(filename);

            remove_filename(&path);

            for (int trackno = 1; trackno < trackcount; trackno++)
            {
                Track *track = cd_get_track(cd, trackno);
                if (track == nullptr)
                {
                    Logging::error(filename, "Unable to get track no. %1 from cue sheet.", trackno);
                    errno = EIO;
                    throw -errno;
                }

                if (!create_cuesheet_virtualfile(track, trackno, filename, path + dirname + "/", statbuf, trackcount, trackno, album, genre, date))
                {
                    //Logging::error(filename, "Failed to create virtual path: %1", subbdir.c_str());
                    throw -errno;
                }
            }
        }

        res = trackcount;
    }
    catch (int _res)
    {
        res = _res;
    }

    if (cd != nullptr)
    {
        cd_delete(cd);
    }

    return res;
}

int check_cuesheet(const std::string & filename, void *buf, fuse_fill_dir_t filler)
{
    std::string _filename(filename);
    struct stat stbuf;
    int res = 0;

    std::string path1(filename);
    remove_filename(&path1);

    std::string path2(filename);

    std::string cuesheet(filename);
    std::string ext;
    if (find_ext(&ext, filename) && ext == TRACKDIR)
    {
        remove_ext(&cuesheet);  // remove TRACKDIR extension from virtual directory
        remove_ext(&_filename);
    }
    else
    {
        append_ext(&path2, TRACKDIR);
        append_sep(&path2);
    }
    replace_ext(&cuesheet, "cue");

    if (lstat(cuesheet.c_str(), &stbuf) != -1)
    {
        if (!check_path(path2))
        {
            res = parse_cuesheet(_filename, cuesheet, &stbuf, buf, filler);
            Logging::trace(cuesheet, "Found %1 titles.", res);
        }
        else
        {
            std::string dirname(path2);

            remove_path(&dirname);

            LPVIRTUALFILE virtualfile = find_file(path2);

            if (virtualfile == nullptr)
            {
                Logging::error(filename, "Failed to find virtual path: %1", path2.c_str());
                errno = EIO;
                return -errno;
            }

            if (buf != nullptr && filler(buf, dirname.c_str(), &virtualfile->m_st, 0))
            {
                // break;
            }

            res = 0;
        }
    }
    return res;
}
