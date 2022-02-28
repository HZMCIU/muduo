// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#pragma once

#include "muduo/base/StringPiece.h"
#include "muduo/base/noncopyable.h"
#include <zlib.h>

namespace muduo {

class GzipFile : noncopyable {
public:
    GzipFile(GzipFile&& rhs) noexcept
        : file_(rhs.file_)
    {
        rhs.file_ = NULL;
    }

    ~GzipFile()
    {
        if (file_) {
            /**
             * @comment gzclose
             * @brief
             * - prototype: int gzclose (gzFile file );
             * - purpose: The gzclose() function shall close the compressed file stream file. If file was open for writing, gzclose() shall first flush any pending output.
             *            Any state information allocated shall be freed.
             */
            ::gzclose(file_);
        }
    }

    GzipFile& operator=(GzipFile&& rhs) noexcept
    {
        swap(rhs);
        return *this;
    }

    bool valid() const
    {
        return file_ != NULL;
    }
    void swap(GzipFile& rhs)
    {
        std::swap(file_, rhs.file_);
    }
#if ZLIB_VERNUM >= 0x1240
    bool setBuffer(int size)
    {
        /**
         * @comment gzbuffer
         * @link  https://zlib.net/manual.html#:~:text=ZEXTERN%20int%20ZEXPORT%20gzbuffer%20OF((gzFile%20file%2C%20unsigned%20size))%3B
         * @brief
         * - prototype: ZEXTERN int ZEXPORT gzbuffer OF((gzFile file, unsigned size));
         * - purpose: Set the internal buffer size used by this library's functions. The default buffer size is 8192 bytes.
         *   This function must be called after gzopen() or gzdopen(), and before any other calls that read or write the file.
         *   The buffer memory allocation is always deferred to the first read or write. Three times that size in buffer space is allocated.
         *   A larger buffer size of, for example, 64K or 128K bytes will noticeably increase the speed of decompression (reading).
         */
        return ::gzbuffer(file_, size) == 0;
    }
#endif

    // return the number of uncompressed bytes actually read, 0 for eof, -1 for error
    int read(void* buf, int len)
    {
        /**
         * @comment gzread
         * @link https://refspecs.linuxbase.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/zlib-gzread-1.html
         * @brief
         * - prototype: int gzread (gzFile file, voidp buf, unsigned int len);
         * - purpose: The gzread() function shall read data from the compressed file referenced by file, which shall have been opened in a read mode (see gzopen() and gzdopen()).
         *   The gzread() function shall read data from file, and uncompress it into buf. At most, len bytes of uncompressed data shall be copied to buf.
         *   If the file is not compressed, gzread() shall simply copy data from file to buf without alteration.
         */
        return ::gzread(file_, buf, len);
    }

    // return the number of uncompressed bytes actually written
    int write(StringPiece buf)
    {
        /**
         * @comment gzwrite
         * @brief
         * - prototype: int gzwrite (gzFile file, voidpc buf, unsigned int len);
         * - purpose: The gzwrite() function shall write data to the compressed file referenced by file, which shall have been opened in a write mode (see gzopen() and gzdopen()).
         *   On entry, buf shall point to a buffer containing lenbytes of uncompressed data. The gzwrite() function shall compress this data and write it to file.
         *   The gzwrite() function shall return the number of uncompressed bytes actually written.
         */
        return ::gzwrite(file_, buf.data(), buf.size());
    }

    // number of uncompressed bytes
    off_t tell() const
    {
        /**
         * @comment gztell
         * @brief
         * - prototype: ZEXTERN z_off_t ZEXPORT gztell OF((gzFile file));
         * - purpose: Returns the starting position for the next gzread or gzwrite on the given compressed file.
         *   This position represents a number of bytes in the uncompressed data stream, and is zero when starting,
         *   even if appending or reading a gzip stream from the middle of a file using gzdopen().
         */
        return ::gztell(file_);
    }

#if ZLIB_VERNUM >= 0x1240
    // number of compressed bytes
    off_t offset() const
    {
        /**
         * @comment gzoffset
         * @brief
         * - prototype: ZEXTERN z_off_t ZEXPORT gzoffset OF((gzFile file));
         * - purpose: Returns the current offset in the file being read or written. This offset includes the count of bytes that precede the gzip stream,
         *   for example when appending or when using gzdopen() for reading. When reading, the offset does not include as yet unused buffered input.
         *   This information can be used for a progress indicator. On error, gzoffset() returns –1.
         */
        return ::gzoffset(file_);
    }
#endif

    // int flush(int f) { return ::gzflush(file_, f); }

    /**
     * @comment gzopen
     * @brief
     * - prototype: ZEXTERN gzFile ZEXPORT gzopen OF((const char *path, const char *mode));
     * - purpose: Opens a gzip (.gz) file for reading or writing. The mode parameter is as in fopen ("rb" or "wb") but can also include a compression level ("wb9")
     *            or a strategy: 'f' for filtered data as in "wb6f", 'h' for Huffman-only compression as in "wb1h", 'R' for run-length encoding as in "wb1R",
     *            or 'F' for fixed code compression as in "wb9F". T' will request transparent writing or appending with no compression and not using the gzip format.
     *
     *            'a' can be used instead of 'w' to request that the gzip stream that will be written be appended to the file. '+' will result in an error,
     *            since reading and writing to the same gzip file is not supported. The addition of 'x' when writing will create the file exclusively,
     *            which fails if the file already exists. On systems that support it,
     *            the addition of 'e' when reading or writing will set the flag to close the file on an execve() call.
     */
    static GzipFile openForRead(StringArg filename)
    {
        return GzipFile(::gzopen(filename.c_str(), "rbe"));
    }

    static GzipFile openForAppend(StringArg filename)
    {
        return GzipFile(::gzopen(filename.c_str(), "abe"));
    }

    static GzipFile openForWriteExclusive(StringArg filename)
    {
        return GzipFile(::gzopen(filename.c_str(), "wbxe"));
    }

    static GzipFile openForWriteTruncate(StringArg filename)
    {
        return GzipFile(::gzopen(filename.c_str(), "wbe"));
    }

private:
    explicit GzipFile(gzFile file)
        : file_(file)
    {
    }

    gzFile file_;
};

}  // namespace muduo
