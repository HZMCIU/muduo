// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CURRENTTHREAD_H
#define MUDUO_BASE_CURRENTTHREAD_H

#include "muduo/base/Types.h"

namespace muduo {
namespace CurrentThread {
// internal
/**
 * @comment __thread keyword
 * @link https://www.ibm.com/docs/fi/xl-c-and-cpp-aix/13.1.0?topic=specifiers-thread-storage-class-specifier-extension
 * @brief The __thread storage class marks a static variable as having thread-local storage duration. 
 * This means that, in a multi-threaded application, a unique instance of the variable is created for each thread that uses it, and destroyed when the thread terminates.
 */
extern __thread int t_cachedTid;
extern __thread char t_tidString[32];
extern __thread int t_tidStringLength;
extern __thread const char* t_threadName;
void cacheTid();

inline int tid()
{
    if (__builtin_expect(t_cachedTid == 0, 0)) {
        // implemented in Thread.cc
        cacheTid();
    }
    return t_cachedTid;
}

inline const char* tidString() // for logging
{
    return t_tidString;
}

inline int tidStringLength() // for logging
{
    return t_tidStringLength;
}

inline const char* name()
{
    return t_threadName;
}

bool isMainThread();

void sleepUsec(int64_t usec);  // for testing

string stackTrace(bool demangle);
}  // namespace CurrentThread
}  // namespace muduo

#endif  // MUDUO_BASE_CURRENTTHREAD_H
