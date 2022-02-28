// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/FileUtil.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;

/**
 * @comment O_CLOEXEC
 * @link
 * - https://man7.org/linux/man-pages/man3/fopen.3.html
 * - https://man7.org/linux/man-pages/man2/open.2.html
 * - https://man7.org/linux/man-pages/man2/fcntl.2.html
 * - https://man7.org/linux/man-pages/man2/execve.2.html
 * @brief
 * - fopen
 *   - 'e' flag for fopen: Open the file with the O_CLOEXEC flag.  See open(2) for more information.  This flag is ignored for fdopen().
 *   - prototype: FILE *fopen(const char *restrict pathname, const char *restrict mode);
 * - open
 *   - prototype: int open(const char *pathname, int flags, mode_t mode);
 *   - O_CLOEXEC: Enable the close-on-exec flag for the new file descriptor.
 *                Specifying this flag permits a program to avoid additional fcntl(2) F_SETFD operations to set the FD_CLOEXEC flag
 *
*                 Note that the use of this flag is essential in some multithreaded programs, because using a separate fcntl(2) F_SETFD operation
*                 to set the FD_CLOEXEC flag does not suffice to avoid race conditions where one thread opens file descriptor and attempts to set its close-on-exec flag
*                 using fcntl(2) at the same time as another thread does a fork(2) plus execve(2).  Depending on the order of execution,
*                 the race may lead to the file descriptor returned by open() being unintentionally leaked to the program executed by the child process created by fork(2).
*                 (This kind of race is in principle possible for any system call that creates a file descriptor whose close-on-exec flag should be set,
*                 and various other Linux system calls provide an equivalent of the O_CLOEXEC flag to deal with this problem.)
*  - fcntl
*    - prototype: int fcntl(int fd, int cmd, ... );
*    - purpose: fcntl() performs one of the operations described below on the open file descriptor fd.  The operation is determined by cmd.
*    - FD_CLOEXEC: Currently, only one such flag is defined: FD_CLOEXEC, the close-on-exec flag.  If the FD_CLOEXEC bit is set,
*                  the file descriptor will automatically be closed during a successful execve(2).
*                  (If the execve(2) fails, the file descriptor is left open.)  If the FD_CLOEXEC bit is not set, the file descriptor will remain open across an execve(2)
*  - execve
*    - prototype: int execve(const char *pathname, char *const argv[], char *const envp[]);
*    - purpose: execve() executes the program referred to by pathname.  This causes the program that is currently being run by the calling process to be replaced with
*               a new program, with newly initialized stack, heap, and (initialized and uninitialized) data segments.
*/
FileUtil::AppendFile::AppendFile(StringArg filename)
    : fp_(::fopen(filename.c_str(), "ae")),  // 'e' for O_CLOEXEC
      writtenBytes_(0)
{
    assert(fp_);
    /**
     * @comment setbuffer
     * @brief
     * - prototype: void setbuffer(FILE *stream, char *buf, size_t size);
     * - purpose: The three types of buffering available are unbuffered, block buffered, and line buffered.
     *            When an output stream is unbuffered, information appears on the destination file or terminal as soon as written;
     *            when it is block buffered many characters are saved up and written as a block;
     *            when it is line buffered characters are saved up until a newline is output or input is read from any stream attached to a terminal device (typically stdin).
     *
     *            The function fflush(3) may be used to force the block out early. (See fclose(3).)
     *            Normally all files are block buffered. When the first I/O operation occurs on a file, malloc(3) is called, and a buffer is obtained.
     *            If a stream refers to a terminal (as stdout normally does) it is line buffered. The standard error stream stderr is always unbuffered by default.
     *
     *            The setvbuf() function may be used on any open stream to change its buffer. The mode argument must be one of the following three macros
     *            _IONBF: unbuffered, _IOLBF: line buffered, _IOFBF:fully buffered
     *
     *            The setvbuf() function may only be used after opening a stream and before any other operations have been performed on it
     *
     *            The other three calls are, in effect, simply aliases for calls to setvbuf(). The setbuf() function is exactly equivalent to the call
     *                          `setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);`
     *
     *            The setbuffer() function is the same, except that the size of the buffer is up to the caller, rather than being determined by the default BUFSIZ.
     */
    ::setbuffer(fp_, buffer_, sizeof buffer_);
    // posix_fadvise POSIX_FADV_DONTNEED ?
}

FileUtil::AppendFile::~AppendFile()
{
    /**
     * @comment fclose
     * @brief
     * - purpose: The fclose() function flushes the stream pointed to by stream (writing any buffered output data using fflush(3)) and closes the underlying file descriptor.
     */
    ::fclose(fp_);
}

void FileUtil::AppendFile::append(const char* logline, const size_t len)
{
    size_t written = 0;

    while (written != len) {
        size_t remain = len - written;
        size_t n = write(logline + written, remain);
        if (n != remain) {
            /**
             * @comment ferror
             * @brief
             * - prototype: int ferror(FILE *stream);
             * - purpose: The function ferror() tests the error indicator for the stream pointed to by stream, returning nonzero if it is set.
             *            The error indicator can be reset only by the clearerr() function.
             */
            int err = ferror(fp_);
            if (err) {
                fprintf(stderr, "AppendFile::append() failed %s\n", strerror_tl(err));
                break;
            }
        }
        written += n;
    }

    writtenBytes_ += written;
}

void FileUtil::AppendFile::flush()
{
    ::fflush(fp_);
}

size_t FileUtil::AppendFile::write(const char* logline, size_t len)
{
    // #undef fwrite_unlocked
    /**
     * @comment fwrite_unlocked
     * @brief
     * - prototype: size_t fwrite_unlocked(const void *ptr, size_t size, size_t n, FILE *stream);
     *   - purpose: Each of these functions has the same behavior as its counterpart without the "_unlocked" suffix,
     *              except that they do not use locking (they do not set locks themselves, and do not test for the presence of locks set by others) and hence are thread-unsafe
     * - prototype: size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
     *   - purpose: The fwrite() function shall write, from the array pointed to by ptr, up to nitems elements whose size is specified by size,
     *              to the stream pointed to by stream.  For each object, size calls shall be made to the fputc() function,
     *              taking the values (in order) from an array of unsigned char exactly overlaying the object.
     *              The file-position indicator for the stream (if defined) shall be advanced by the number of bytes successfully written.
     *              If an error occurs, the resulting value of the file-position indicator for the stream is unspecified.
     */
    return ::fwrite_unlocked(logline, 1, len, fp_);
}

/**
 * @comment O_RDONLY
 * @link https://man7.org/linux/man-pages/man2/open.2.html
 * @brief
 * - prototype: int open(const char *pathname, int flags);
 * - purpose: The argument flags must include one of the following access modes: O_RDONLY, O_WRONLY, or O_RDWR.
 *            These request opening the file read-only, write-only, or read/write, respectively.
 */
FileUtil::ReadSmallFile::ReadSmallFile(StringArg filename)
    : fd_(::open(filename.c_str(), O_RDONLY | O_CLOEXEC)),
      err_(0)
{
    buf_[0] = '\0';
    if (fd_ < 0) {
        err_ = errno;
    }
}

FileUtil::ReadSmallFile::~ReadSmallFile()
{
    if (fd_ >= 0) {
        ::close(fd_); // FIXME: check EINTR
    }
}

// return errno
template<typename String>
int FileUtil::ReadSmallFile::readToString(int maxSize,
        String* content,
        int64_t* fileSize,
        int64_t* modifyTime,
        int64_t* createTime)
{
    static_assert(sizeof(off_t) == 8, "_FILE_OFFSET_BITS = 64");
    assert(content != NULL);
    int err = err_;
    if (fd_ >= 0) {
        content->clear();

        // if argument fileSize if nullptr then don't need to obtain the file status.
        if (fileSize) {
            /**
             * @comment struct stat
             * @brief
             * - prototype:
             * struct stat {
             *      dev_t     st_dev;         // ID of device containing file
             *      ino_t     st_ino;         // Inode number
             *      mode_t    st_mode;        // File type and mode
             *      nlink_t   st_nlink;       // Number of hard links
             *      uid_t     st_uid;         // User ID of owner
             *      gid_t     st_gid;         // Group ID of owner
             *      dev_t     st_rdev;        // Device ID (if special file)
             *      off_t     st_size;        // Total size, in bytes
             *      blksize_t st_blksize;     // Block size for filesystem I/O
             *      blkcnt_t  st_blocks;      // Number of 512B blocks allocated
             *
             *         Since Linux 2.6, the kernel supports nanosecond
             *         precision for the following timestamp fields.
             *         For the details before Linux 2.6, see NOTES.
             *
             *      struct timespec st_atim;  // Time of last access
             *      struct timespec st_mtim;  // Time of last modification
             *      struct timespec st_ctim;  // Time of last status change

             *      #define st_atime st_atim.tv_sec      // Backward compatibility
             *      #define st_mtime st_mtim.tv_sec
             *      #define st_ctime st_ctim.tv_sec
             * };
             */
            struct stat statbuf;
            /**
             * @comment fstat
             * @brief
             * - prototype: int fstat(int fd, struct stat *statbuf);
             * - purpose: These functions return information about a file, in the buffer pointed to by statbuf.
             */
            if (::fstat(fd_, &statbuf) == 0) {
                /**
                 * @comment S_ISREG
                 * @link https://stackoverflow.com/questions/40163270/what-is-s-isreg-and-what-does-it-do
                 * @brief
                 * - purpose: S_ISREG() is a macro used to interpret the values in a stat-struct, as returned from the system call stat().
                 *            It evaluates to true if the argument(The st_mode member in struct stat) is a regular file.
                 */
                if (S_ISREG(statbuf.st_mode)) {
                    *fileSize = statbuf.st_size;
                    /**
                     * @comment static_cast, implicit_cast
                     * @link
                     * - https://en.cppreference.com/w/cpp/language/static_cast,
                     * - https://stackoverflow.com/questions/868306/what-is-the-difference-between-static-cast-and-implicit-cast
                     */
                    content->reserve(static_cast<int>(std::min(implicit_cast<int64_t>(maxSize), *fileSize)));
                }
                else if (S_ISDIR(statbuf.st_mode)) {
                    err = EISDIR;
                }
                if (modifyTime) {
                    *modifyTime = statbuf.st_mtime;
                }
                if (createTime) {
                    *createTime = statbuf.st_ctime;
                }
            }
            else {
                err = errno;
            }
        }

        while (content->size() < implicit_cast<size_t>(maxSize)) {
            size_t toRead = std::min(implicit_cast<size_t>(maxSize) - content->size(), sizeof(buf_));
            ssize_t n = ::read(fd_, buf_, toRead);
            if (n > 0) {
                content->append(buf_, n);
            }
            else {
                if (n < 0) {
                    err = errno;
                }
                break;
            }
        }
    }
    return err;
}

int FileUtil::ReadSmallFile::readToBuffer(int* size)
{
    int err = err_;
    if (fd_ >= 0) {
        /**
         * @comment pread
         * @link https://stackoverflow.com/questions/1687275/what-is-the-difference-between-read-and-pread-in-unix
         * @brief
         * - prototype: ssize_t pread(int fd, void *buf, size_t count, off_t offset);
         * - purpose:  pread() reads up to count bytes from file descriptor fd at offset offset (from the start of the file) into the buffer starting at buf.
         *             The file offset is not changed.
         * - Pread() works just like read() but reads from the specified position in the file **without** modifying the file pointer.
         */
        ssize_t n = ::pread(fd_, buf_, sizeof(buf_) - 1, 0);
        if (n >= 0) {
            if (size) {
                *size = static_cast<int>(n);
            }
            buf_[n] = '\0';
        }
        else {
            err = errno;
        }
    }
    return err;
}

template int FileUtil::readFile(StringArg filename,
                                int maxSize,
                                string* content,
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::ReadSmallFile::readToString(
    int maxSize,
    string* content,
    int64_t*, int64_t*, int64_t*);
