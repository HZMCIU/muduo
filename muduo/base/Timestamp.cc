// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Timestamp.h"

#include <sys/time.h>
#include <stdio.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>

using namespace muduo;

static_assert(sizeof(Timestamp) == sizeof(int64_t),
              "Timestamp should be same size as int64_t");

string Timestamp::toString() const
{
    char buf[32] = {0};
    int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;
    int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;
    snprintf(buf, sizeof(buf), "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
    return buf;
}

string Timestamp::toFormattedString(bool showMicroseconds) const
{
    char buf[64] = {0};
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    struct tm tm_time;
    gmtime_r(&seconds, &tm_time);

    if (showMicroseconds) {
        int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
                 microseconds);
    }
    else {
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return buf;
}

Timestamp Timestamp::now()
{
    struct timeval tv;
    /**
     * @comment gettimeofday
     * @brief
     * - prototype: int gettimeofday(struct timeval *restrict tv, struct timezone *restrict tz);
     * - purpose: The functions gettimeofday() and settimeofday() can get and set the time as well as a timezone.
     *            The tv argument is a struct timeval (as specified in
     *
     *            The tv argument is a struct timeval (as specified in
     *            <sys/time.h>):
     * 
     *            struct timeval {
     *              time_t      tv_sec;     // seconds
     *              suseconds_t tv_usec;    // microseconds
     *            };
     * 
     *            and gives the number of seconds and microseconds since the Epoch (see time(2)).
     *
     *            The tz argument is a struct timezone:
     *
     *            struct timezone {
     *                int tz_minuteswest;     // minutes west of Greenwich
     *                int tz_dsttime;         // type of DST correction 
     *            };
     *
     *            If either tv or tz is NULL, the corresponding structure is not set or returned.  (However, compilation warnings will result if tv is NULL.)
     */
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}
