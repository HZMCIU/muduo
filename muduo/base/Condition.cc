// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Condition.h"

#include <errno.h>

// returns true if time out, false otherwise.
bool muduo::Condition::waitForSeconds(double seconds)
{
    /**
     * @comment struct timespec
     * @link https://en.cppreference.com/w/c/chrono/timespec
     * @brief
     * - Structure holding an interval broken down into seconds and nanoseconds.
     * - member object:
     *   - time_t tv_sec	whole seconds (valid values are >= 0)
     *   - long tv_nsec	nanoseconds (valid values are [0, 999999999])
     */
    struct timespec abstime;
    // FIXME: use CLOCK_MONOTONIC or CLOCK_MONOTONIC_RAW to prevent time rewind.
    /**
     * @comment clock_gettime
     * @brief
     * - prototype: int clock_gettime(clockid_t clockid, struct timespec *tp);
     * - The functions clock_gettime() and clock_settime() retrieve and set
     *   the time of the specified clock clockid.
     * - The clockid argument is the identifier of the particular clock on which to act.
     *   A clock may be system-wide and hence visible for all processes, or per-process if it measures time only within a single process.
     * - All implementations support the system-wide real-time clock, which is identified by CLOCK_REALTIME.
     *   Its time represents seconds and nanoseconds since the Epoch. When its time is changed, timers for a relative interval are unaffected,
     *   but timers for an absolute point in time are affected.
     * - A settable system-wide clock that measures real (i.e., wallclock) time.
     *   Setting this clock requires appropriate privileges.
     *   This clock is affected by discontinuous jumps in the system time
     *   (e.g., if the system administrator manually changes the clock), and by the incremental adjustments performed by adjtime(3) and NTP.
     */
    clock_gettime(CLOCK_REALTIME, &abstime);

    const int64_t kNanoSecondsPerSecond = 1000000000;
    int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);

    abstime.tv_sec += static_cast<time_t>((abstime.tv_nsec + nanoseconds) / kNanoSecondsPerSecond);
    abstime.tv_nsec = static_cast<long>((abstime.tv_nsec + nanoseconds) % kNanoSecondsPerSecond);

    MutexLock::UnassignGuard ug(mutex_);
    /**
     * @comment pthread_cond_timedwait
     * @link https://pubs.opengroup.org/onlinepubs/009604599/functions/pthread_cond_timedwait.html
     * @brief 
     * - prototype: int pthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime);
     * - The pthread_cond_timedwait() function shall be equivalent to pthread_cond_wait(), 
     *   except that an error is returned if the absolute time specified by abstime passes (that is, system time equals or exceeds abstime) 
     *   before the condition cond is signaled or broadcasted, or if the absolute time specified by abstime has already been passed at the time of the call.
     */
    return ETIMEDOUT == pthread_cond_timedwait(&pcond_, mutex_.getPthreadMutex(), &abstime);
}
