// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "muduo/net/TimerQueue.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"

#include <sys/timerfd.h>
#include <unistd.h>

namespace muduo {
namespace net {
namespace detail {

int createTimerfd()
{
    /**
     * @comment timerfd_create
     * @brief
     * - prototype: int timerfd_create(int clockid, int flags);
     * - purpose: timerfd_create() creates a new timer object, and returns a file descriptor that refers to that timer.
     *            The clockid argument specifies the clock that is used to mark the progress of the timer, and must be one of the following:
     *   - CLOCK_REALTIME: A settable system-wide real-time clock
     *   - CLOCK_MONOTONIC: A nonsettable monotonically increasing clock that measures time from some unspecified point in the past that does not change after system startup.
     *   - TFD_NONBLOCK: Set the O_NONBLOCK file status flag on the open file description (see open(2)) referred to by the new file descriptor.
     *                   Using this flag saves extra calls to fcntl(2) to achieve the same result.
     *   - TFD_CLOEXEC: Set the close-on-exec (FD_CLOEXEC) flag on the new file descriptor.  See the description of the O_CLOEXEC flag in open(2)
     *                  for reasons why this may be useful.
     */
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                   TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        LOG_SYSFATAL << "Failed in timerfd_create";
    }
    return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.microSecondsSinceEpoch()
                           - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100) {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
                    microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
                     (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
    if (n != sizeof howmany) {
        LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    /**
     * @comment struct itimerspec
     * @brief
     * - prototype:
     *     struct timespec {
     *         time_t tv_sec;                // Seconds
     *         long   tv_nsec;               // Nanoseconds
     *     };
     *
     *     struct itimerspec {
     *         struct timespec it_interval;  // Timer interval
     *         struct timespec it_value;     // Initial expiration
     *     };
     */
    // wake up loop by timerfd_settime()
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memZero(&newValue, sizeof newValue);
    memZero(&oldValue, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    /**
     * @comment timerfd_settime
     * @brief
     * - prototype: int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);
     * - purpose: timer_settime() arms or disarms the timer identified by timerid. The new_value argument is pointer to an itimerspec structure that specifies
     *            the new initial value and the new interval for the timer.
     */
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret) {
        LOG_SYSERR << "timerfd_settime()";
    }
}

}  // namespace detail
}  // namespace net
}  // namespace muduo

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      timers_(),
      callingExpiredTimers_(false)
{
    timerfdChannel_.setReadCallback(
        std::bind(&TimerQueue::handleRead, this));
    // we are always reading the timerfd, we disarm it with timerfd_settime.
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
    // do not remove channel, since we're in EventLoop::dtor();
    for (const Entry& timer : timers_) {
        delete timer.second;
    }
}

TimerId TimerQueue::addTimer(TimerCallback cb,
                             Timestamp when,
                             double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(
        std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop(
        std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);

    if (earliestChanged) {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end()) {
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1);
        (void)n;
        delete it->first; // FIXME: no delete please
        activeTimers_.erase(it);
    }
    else if (callingExpiredTimers_) {
        cancelingTimers_.insert(timer);
    }
    assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);

    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    // safe to callback outside critical section
    for (const Entry& it : expired) {
        it.second->run();
    }
    callingExpiredTimers_ = false;

    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    TimerList::iterator end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now < end->first);
    /**
     * @comment back_inserter
     * @brief
     * - prototype: 
     *   template< class Container >
     *   std::back_insert_iterator<Container> back_inserter( Container& c );
     * - purpose:
     *   back_inserter is a convenient function template that constructs a std::back_insert_iterator for the container c with the type deduced from the type of the argument.
     */
    std::copy(timers_.begin(), end, back_inserter(expired));
    timers_.erase(timers_.begin(), end);

    for (const Entry& it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
        (void)n;
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;

    for (const Entry& it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        if (it.second->repeat()
                && cancelingTimers_.find(timer) == cancelingTimers_.end()) {
            it.second->restart(now);
            insert(it.second);
        }
        else {
            // FIXME move to a free list
            delete it.second; // FIXME: no delete please
        }
    }

    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid()) {
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer* timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first) {
        earliestChanged = true;
    }
    {
        std::pair<TimerList::iterator, bool> result
            = timers_.insert(Entry(when, timer));
        assert(result.second);
        (void)result;
    }
    {
        std::pair<ActiveTimerSet::iterator, bool> result
            = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}
