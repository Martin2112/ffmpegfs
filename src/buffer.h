/*
 * Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
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
 * @file buffer.h
 * @brief Buffer class
 *
 * @ingroup ffmpegfs
 *
 * @author Norbert Schlia (nschlia@oblivion-software.de)
 * @copyright Copyright (C) 2017-2022 Norbert Schlia (nschlia@oblivion-software.de)
 */

#ifndef BUFFER_H
#define BUFFER_H

#pragma once

#include "fileio.h"

#include <mutex>
#include <vector>
#include <stddef.h>

#define CACHE_CHECK_BIT(mask, var)  ((mask) == (mask & (var)))  /**< @brief Check bit in bitmask */

#define CACHE_CLOSE_NOOPT   0x00000000                      /**< @brief Dummy, do nothing special */
#define CACHE_CLOSE_FREE    0x00000001                      /**< @brief Free memory for cache entry */
#define CACHE_CLOSE_DELETE  (0x00000002 | CACHE_CLOSE_FREE) /**< @brief Delete cache entry, will unlink cached file! Implies CACHE_CLOSE_FREE. */

#define CACHE_FLAG_RO       0x00000001                      /**< @brief Mark cache file read-only */
#define CACHE_FLAG_RW       0x00000002                      /**< @brief Mark cache file writeable, implies read permissions */

/**
 * @brief The #Buffer class
 */
class Buffer : public FileIO
{
public:
    /**
     * @brief Structure to hold current cache state
     */
    typedef struct _tagCACHEINFO
    {
    public:
        _tagCACHEINFO()
            : m_fd(-1)
            , m_buffer(nullptr)
            , m_buffer_pos(0)
            , m_buffer_watermark(0)
            , m_buffer_size(0)
            , m_seg_finished(false)
            , m_fd_idx(-1)
            , m_buffer_idx(nullptr)
            , m_buffer_size_idx(0)
            , m_flags(0)
        {
        }

        // Main cache
        std::string             m_cachefile;                    /**< @brief Cache file name */
        int                     m_fd;                           /**< @brief File handle for buffer */
        uint8_t *               m_buffer;                       /**< @brief Pointer to buffer memory */
        size_t                  m_buffer_pos;                   /**< @brief Read/write position */
        size_t                  m_buffer_watermark;             /**< @brief Number of bytes in buffer */
        size_t                  m_buffer_size;                  /**< @brief Current buffer size */
        bool                    m_seg_finished;                 /**< @brief True if segment completely decoded */
        // Index for frame sets
        std::string             m_cachefile_idx;                /**< @brief Index file name */
        int                     m_fd_idx;                       /**< @brief File handle for index */
        uint8_t *               m_buffer_idx;                   /**< @brief Pointer to index memory */
        size_t                  m_buffer_size_idx;              /**< @brief Size of index buffer */
        // Flags
        uint32_t                m_flags;                        /**< @brief CACHE_FLAG_* options */
    } CACHEINFO, *LPCACHEINFO;                                  /**< @brief Pointer version of CACHEINFO */
    typedef CACHEINFO const * LPCCACHEINFO;                     /**< @brief Pointer to const version of CACHEINFO */

public:
    /**
     * @brief Create #Buffer object
     */
    explicit Buffer();
    /**
     * @brief Free #Buffer object
     *
     * Release memory and close files
     */
    virtual ~Buffer();
    /**
     * @brief Get type of this virtual file.
     * @return Returns VIRTUALTYPE_BUFFER.
     */
    virtual VIRTUALTYPE     type() const override;
    /**
     * @brief Initialise cache
     * @param[in] erase_cache - if true delete old file before opening.
     * @return Returns true on success; false on error.
     */
    bool                    init(bool erase_cache);
    /**
     * @brief Set the current segment
     * @param[in] segment_no - HLS segment file number [1..n].
     * @return Returns true on success; if segment_no is 0 or greated then segment_count() returns false and sets errno to EINVALID.
     */
    bool                    set_segment(uint32_t segment_no);
    /**
     * @brief Get segment count.
     * @return Number of segments.
     */
    uint32_t                segment_count();
    /**
     * @brief Get currently selected segment.
     * @return Current segment number [1..n] or 0 if none selected.
     */
    uint32_t                current_segment_no();
    /**
     * @brief Check if segment exists.
     * Returns true if it exists, or false if it still has to be decoded.
     * @param[in] segment_no - HLS segment file number [1..n].
     * @return Returns true if it exists, or false if not.
     */
    bool                    segment_exists(uint32_t segment_no);
    /**
     * @brief Release cache buffer.
     * @param[in] flags - One of the CACHE_CLOSE_* flags.
     * @return Returns true on success; false on error.
     */
    bool                    release(int flags = CACHE_CLOSE_NOOPT);
    /**
     * @brief Size of this buffer.
     * @return Not applicable, returns 0.
     */
    virtual size_t          bufsize() const override;

    /** @brief Open a virtual file
     * @param[in] virtualfile - LPCVIRTUALFILE of file to open
     * @return Upon successful completion, #open() returns 0.
     * On error, an nonzero value is returned and errno is set to indicate the error.
     */
    virtual int             open(LPVIRTUALFILE virtualfile) override;
    /**
     * @brief Not implemented.
     * @param[out] data - unused
     * @param[in] size - unused
     * @return Always returns 0 and errno is EPERM.
     */
    virtual size_t          read(void *data, size_t size) override;
    /**
    * @brief Write image data for the frame number into the Buffer
     * @param[out] data - Buffer to read data in.
     * @param[in] frame_no - Number of the frame to write.
     * @return Upon successful completion, #read() returns the number of bytes read. @n
     * This may be less than size. @n
     * On error, the value 0 is returned and errno is set to indicate the error. @n
     * If at end of file, 0 may be returned by errno not set. error() will return 0 if at EOF.
     * If the image frame is not yet read, the function also returns 0 and errno is EAGAIN.
     */
    size_t                  read_frame(std::vector<uint8_t> * data, uint32_t frame_no);
    /**
     * @brief Get last error.
     * @return errno value of last error.
     */
    virtual int             error() const override;
    /** @brief Get the duration of the file, in AV_TIME_BASE fractional seconds.
     * @return Not applicable to buffer, always returns AV_NOPTS_VALUE.
     */
    virtual int64_t         duration() const override;
    /**
     * @brief Get the value of the internal buffer size pointer.
     * @return Returns the value of the internal buffer size pointer.
     */
    virtual size_t          size() const override;
    /**
     * @brief Get the value of the internal buffer size pointer.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Returns the value of the internal buffer size pointer.
     */
    virtual size_t          size(uint32_t segment_no) const;
    /**
     * @brief Get the value of the internal read position pointer.
     * @return Returns the value of the internal read position pointer.
     */
    virtual size_t          tell() const override;
    /**
     * @brief Get the value of the internal read position pointer.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Returns the value of the internal read position pointer.
     */
    virtual size_t          tell(uint32_t segment_no) const;
    /** @brief Seek to position in file
     *
     * Repositions the offset of the open file to the argument offset according to the directive whence.
     * May block for a long time if buffer has not been filled to the requested offset.
     *
     * @param[in] offset - offset in bytes
     * @param[in] whence - how to seek: @n
     * SEEK_SET: The offset is set to offset bytes. @n
     * SEEK_CUR: The offset is set to its current location plus offset bytes. @n
     * SEEK_END: The offset is set to the size of the file plus offset bytes.
     * @return Upon successful completion, #seek() returns the resulting offset location as measured in bytes
     * from the beginning of the file.
     */
    virtual int             seek(int64_t offset, int whence) override;
    /** @brief Seek to position in file
     *
     * Repositions the offset of the open file to the argument offset according to the directive whence.
     * May block for a long time if buffer has not been filled to the requested offset.
     *
     * @param[in] offset - offset in bytes
     * @param[in] whence - how to seek: @n
     * SEEK_SET: The offset is set to offset bytes. @n
     * SEEK_CUR: The offset is set to its current location plus offset bytes. @n
     * SEEK_END: The offset is set to the size of the file plus offset bytes.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Upon successful completion, #seek() returns the resulting offset location as measured in bytes
     * from the beginning of the file.
     */
    virtual int             seek(int64_t offset, int whence, uint32_t segment_no);
    /**
     * @brief Check if at end of file.
     * @return Returns true if at end of buffer.
     */
    virtual bool            eof() const override;
    /**
     * @brief Check if at end of file.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Returns true if at end of buffer.
     */
    virtual bool            eof(uint32_t segment_no) const;
    /**
     * @brief Close buffer.
     */
    virtual void            close() override;
    /**
     * @brief Write data to the current position into the buffer. The position pointer will be updated.
     * @param[in] data - Buffer with data to write.
     * @param[in] length - Length of buffer to write.
     * @return Returns the bytes written to the buffer. If less than length this indicates an error, consult errno for details.
     */
    size_t                  write(const uint8_t* data, size_t length);
    /**
    * @brief Write image data for the frame number into the buffer.
     * @param[in] data - Buffer with data to write.
     * @param[in] length - Length of buffer to write.
     * @param[in] frame_no - Number of the frame to write.
     * @return Returns the bytes written to the buffer. If less than length this indicates an error, consult errno for details.
     */
    size_t                  write_frame(const uint8_t* data, size_t length, uint32_t frame_no);
    /**
     * @brief Flush buffer to disk
     * @return Returns true on success; false on error. Check errno for details.
     */
    bool                    flush();
    /**
     * @brief Clear (delete) buffer.
     * @return Returns true on success; false on error. Check errno for details.
     */
    bool                    clear();
    /**
     * @brief Reserve memory without changing size to reduce re-allocations.
     * @param[in] size - Size of buffer to reserve.
     * @return Returns true on success; false on error.
     */
    bool                    reserve(size_t size);
    /** @brief Return the current watermark of the file while transcoding
     *
     * While transcoding, this value reflects the current size of the transcoded file.
     * This is the maximum byte offset until the file can be read so far.
     *
     * @param[in] segment_no - If > 0 returns watermark for specific segment.
     * If 0, returns watermark for current write segment.
     *  @return Returns the current watermark.
     */
    size_t                  buffer_watermark(uint32_t segment_no = 0) const;
    /**
     * @brief Copy buffered data into output buffer.
     * @param[in] out_data - Buffer to copy data to.
     * @param[in] offset - Offset in buffer to copy data from.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Returns true on success; false on error.
     */
    bool                    copy(std::vector<uint8_t> * out_data, size_t offset, uint32_t segment_no = 0);
    /**
     * @brief Copy buffered data into output buffer.
     * @param[in] out_data - Buffer to copy data to.
     * @param[in] offset - Offset in buffer to copy data from.
     * @param[in] bufsize - Size of out_data buffer.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Returns true on success; false on error.
     */
    bool                    copy(uint8_t* out_data, size_t offset, size_t bufsize, uint32_t segment_no = 0);
    /**
     * @brief Get cache filename.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Returns cache filename.
     */
    const std::string &     cachefile(uint32_t segment_no) const;
    /**
     * @brief Make up a cache file name including full path
     * @param[out] cachefile - Name of cache file.
     * @param[in] filename - Source file name.
     * @param[in] fileext - File extension (MP4, WEBM etc.).
     * @param[in] is_idx - If true, create index file; otherwise create a cache.
     * @return Returns the name of the cache file.
     */
    static const std::string & make_cachefile_name(std::string &cachefile, const std::string & filename, const std::string &fileext, bool is_idx);
    /**
     * @brief Remove (unlink) file.
     * @param[in] filename - Name of file to remove.
     * @return Returns true on success; false on error.
     */
    static bool             remove_file(const std::string & filename);
    /**
     * @brief Check if we have the requested frame number. Works only when processing a frame set.
     * @param[in] frame_no - 1...frames
     * @return Returns true of frame is already in cache, false if not.
     */
    bool                    have_frame(uint32_t frame_no);
    /**
     * @brief Finish decoded segment
     */
    void                    finished_segment();
    /**
     * @brief Return true if transcoding segement finished.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Returns true if finished, false if not.
     */
    bool                    is_segment_finished(uint32_t segment_no) const;
    /**
     * @brief Open cache file if not already open.
     * @param[in] segment_no - Index of segment file number [0..n-1].
     * @param[in] flags - CACHE_FLAG_* options
     * @return Returns true on success or file already open; false on error.
     */
    bool                    open_file(uint32_t segment_no, uint32_t flags);
    /**
     * @brief Close cache file if not already closed.
     * @param[in] segment_no - Index of segment file number [0..n-1].
     * @param[in] flags - CACHE_FLAG_* options
     * @return Returns true on success or file already closed; false on error.
     */
    bool                    close_file(uint32_t segment_no, uint32_t flags);

protected:
    /**
     * @brief Remove the cachefile.
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return Returns true on success; false on error.
     */
    bool                    remove_cachefile(uint32_t segment_no = 0) const;
    /**
     * @brief Check if the cache file is open
     * @return Returns true if the cache file is open; false if not.
     */
    bool                    is_open();

private:
    /**
     * @brief Prepare write operation.
     *
     * Ensure the Buffer has sufficient space for a quantity of data and
     * return a pointer where the data may be written. The position pointer
     * should be updated afterward with increment_pos().
     * @param[in] length - Length of buffer to prepare.
     * @return Returns a pointer to the memory to write.
     */
    uint8_t*                write_prepare(size_t length);
    /**
     * @brief Increment buffer position.
     *
     * Increment the location of the internal pointer. This cannot fail and so
     * returns void. It does not ensure the position is valid memory because
     * that is done by the write_prepare methods via reallocate.
     * @param[in] increment - Increment size.
     */
    void                    increment_pos(size_t increment);
    /**
     * @brief Reallocate buffer to new size.
     *
     * Ensure the allocation has at least size bytes available. If not,
     * reallocate memory to make more available. Fill the newly allocated memory
     * with zeroes.
     * @param[in] newsize - New buffer size
     * @return Returns true on success; false on error.
     */
    bool                    reallocate(size_t newsize);
    /**
     * @brief Map memory to file.
     * @param[in] filename - Name of cache file to open.
     * @param[out] fd - The file descriptor of the open cache file.
     * @param[out] p - Memory pointer to cache file.
     * @param[out] filesize - Actual size of the cache file after this call.
     * @param[inout] isdefaultsize -@n
     * In: If false, the file size will be the size of the existing file, returning the size in filesize. If the file does not exist, it will be sized to defaultsize.
     * If true, the defaultsize will be used in any case, resizing an existing file if necessary.@n
     * Out: true if the file size was set to default.
     * @param[out] defaultsize - Default size of the file if it does not exist. This parameter can be zero in which case the size will be set to the system's page size.
     * @param[out] truncate - If true, truncate file when opened.
     * @return Returns true if successful and fd, p, filesize, isdefaultsize filled in or false on error.
     */
    bool                    map_file(const std::string & filename, int *fd, uint8_t **p, size_t *filesize, bool *isdefaultsize, off_t defaultsize, bool truncate) const;
    /**
     * @brief Unmap memory from file.
     * @param[in] filename - Name of cache file to unmap.
     * @param[in] fd - The file descriptor of the open cache file.
     * @param[in] p - Memory pointer to cache file.
     * @param[in] filesize - Actual size of the cache file.
     * @return Returns true on success; false on error.
     */
    bool                    unmap_file(const std::string & filename, int *fd, uint8_t **p, size_t *filesize) const;

    /**
     * @brief cacheinfo
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment or 0 for current segment.
     * @return
     */
    LPCACHEINFO             cacheinfo(uint32_t segment_no);
    /**
     * @brief cacheinfo
     * @param[in] segment_no - HLS segment file number [1..n] or 0 for current segment.
     * @return
     */
    LPCCACHEINFO            const_cacheinfo(uint32_t segment_no) const;

private:
    std::recursive_mutex    m_mutex;                            /**< @brief Access mutex */
    LPCACHEINFO             m_cur_ci;                           /**< @brief Convenience pointer to current write segment */
    uint32_t                m_cur_open;                         /**< @brief Number of open files */

    std::vector<CACHEINFO>  m_ci;                               /**< @brief Cache info */
};

#endif
