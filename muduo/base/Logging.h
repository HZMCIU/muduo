// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_LOGGING_H
#define MUDUO_BASE_LOGGING_H

#include "muduo/base/LogStream.h"
#include "muduo/base/Timestamp.h"

namespace muduo {

class TimeZone;

class Logger {
public:
    enum LogLevel {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS,
    };

    // compile time calculation of basename of source file
    class SourceFile {
    public:
        /**
         * @comment array reference
         * @link https://stackoverflow.com/questions/10007986/c-pass-an-array-by-reference
         * @brief
         * Arrays can only be passed by reference, actually:
         * ```cpp
         * void foo(double (&bar)[10])
         * {
         *
         * }
         * ```
         * This prevents you from doing things like:
         * ```cpp
         * double arr[20];
         * foo(arr); // won't compile
         * ```
         * To be able to pass an arbitrary size array to foo, make it a template and capture the size of the array at compile time:
         * ```cpp
         * template<typename T, size_t N>
         * void foo(T (&bar)[N])
         * {
         *     // use N here
         * }
         * ```
         */
        template<int N>
        SourceFile(const char (&arr)[N])
            : data_(arr),
              size_(N - 1)
        {
            /**
             * @comment strchr
             * @link https://en.cppreference.com/w/cpp/string/byte/strchr
             * @brief
             * - prototype: const char* strchr( const char* str, int ch );
             * - purpose: Finds the first occurrence of the character static_cast<char>(ch) in the byte string pointed to by str.
             * The terminating null character is considered to be a part of the string and can be found if searching for '\0'.
             */
            const char* slash = strrchr(data_, '/'); // builtin function
            if (slash) {
                data_ = slash + 1;
                size_ -= static_cast<int>(data_ - arr);
            }
        }

        explicit SourceFile(const char* filename)
            : data_(filename)
        {
            const char* slash = strrchr(filename, '/');
            if (slash) {
                data_ = slash + 1;
            }
            size_ = static_cast<int>(strlen(data_));
        }

        const char* data_;
        int size_;
    };

    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line, LogLevel level, const char* func);
    Logger(SourceFile file, int line, bool toAbort);
    ~Logger();

    LogStream& stream()
    {
        return impl_.stream_;
    }

    static LogLevel logLevel();
    static void setLogLevel(LogLevel level);

    typedef void (*OutputFunc)(const char* msg, int len);
    typedef void (*FlushFunc)();
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);
    static void setTimeZone(const TimeZone& tz);

private:

    /**
     * @question why design a separte implementation class
     */
    class Impl {
    public:
        typedef Logger::LogLevel LogLevel;
        Impl(LogLevel level, int old_errno, const SourceFile& file, int line);
        void formatTime();
        void finish();

        Timestamp time_;
        LogStream stream_;
        LogLevel level_;
        int line_;
        SourceFile basename_;
    };

    Impl impl_;

};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel()
{
    return g_logLevel;
}

//
// CAUTION: do not write:
//
// if (good)
//   LOG_INFO << "Good news";
// else
//   LOG_WARN << "Bad news";
//
// this expends to
//
// if (good)
//   if (logging_INFO)
//     logInfoStream << "Good news";
//   else
//     logWarnStream << "Bad news";
//

/**
 * @comment log hierarchy order
 * @link https://stackoverflow.com/questions/7745885/log4j-logging-hierarchy-order
 */
#define LOG_TRACE if (muduo::Logger::logLevel() <= muduo::Logger::TRACE) \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::TRACE, __func__).stream()
#define LOG_DEBUG if (muduo::Logger::logLevel() <= muduo::Logger::DEBUG) \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::DEBUG, __func__).stream()
#define LOG_INFO if (muduo::Logger::logLevel() <= muduo::Logger::INFO) \
  muduo::Logger(__FILE__, __LINE__).stream()
#define LOG_WARN muduo::Logger(__FILE__, __LINE__, muduo::Logger::WARN).stream()
#define LOG_ERROR muduo::Logger(__FILE__, __LINE__, muduo::Logger::ERROR).stream()
#define LOG_FATAL muduo::Logger(__FILE__, __LINE__, muduo::Logger::FATAL).stream()
#define LOG_SYSERR muduo::Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL muduo::Logger(__FILE__, __LINE__, true).stream()

const char* strerror_tl(int savedErrno);

// Taken from glog/logging.h
//
// Check that the input is non NULL.  This very useful in constructor
// initializer lists.

/**
 * @comment macro operator
 * @link https://en.cppreference.com/w/cpp/preprocessor/replace
 * @brief 
 * In function-like macros, a # operator before an identifier in the replacement-list runs the identifier through parameter replacement and encloses the result in quotes, 
 * effectively creating a string literal.
 */
#define CHECK_NOTNULL(val) \
  ::muduo::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

// A small helper for CHECK_NOTNULL().
template <typename T>
T* CheckNotNull(Logger::SourceFile file, int line, const char *names, T* ptr)
{
    if (ptr == NULL) {
        Logger(file, line, Logger::FATAL).stream() << names;
    }
    return ptr;
}

}  // namespace muduo

#endif  // MUDUO_BASE_LOGGING_H
