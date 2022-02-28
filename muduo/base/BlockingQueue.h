// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"

#include <deque>
#include <assert.h>

namespace muduo {

template<typename T>
class BlockingQueue : noncopyable {
public:
    using queue_type = std::deque<T>;

    BlockingQueue()
        : mutex_(),
          notEmpty_(mutex_),
          queue_()
    {
    }

    void put(const T& x)
    {
        MutexLockGuard lock(mutex_);
        queue_.push_back(x);
        notEmpty_.notify(); // wait morphing saves us
        // http://www.domaigne.com/blog/computing/condvars-signal-with-mutex-locked-or-not/
    }

    void put(T&& x)
    {
        MutexLockGuard lock(mutex_);
        /**
         * @comment std::move
         * @link https://en.cppreference.com/w/cpp/utility/move, https://stackoverflow.com/questions/3413470/what-is-stdmove-and-when-should-it-be-used
         * @brief
         * - The first thing to note is that std::move() doesn't actually move anything.
         *   It changes an expression from being an lvalue (such as a named variable) to being an xvalue.
         *   an xvalue (an “eXpiring” value) is a glvalue that denotes an object whose resources can be reused;
         */
        queue_.push_back(std::move(x));
        notEmpty_.notify();
    }

    T take()
    {
        /**
         * @comment condition and mutex usage.
         * @ref Condition.h
         * @brief pthread_cond_wait will release the mutex it holds when enters into sleep.
         */
        MutexLockGuard lock(mutex_);
        // always use a while-loop, due to spurious wakeup
        while (queue_.empty()) {
            notEmpty_.wait();
        }
        assert(!queue_.empty());
        T front(std::move(queue_.front()));
        queue_.pop_front();
        return front;
    }

    queue_type drain()
    {
        std::deque<T> queue;
        {
            MutexLockGuard lock(mutex_);
            queue = std::move(queue_);
            assert(queue_.empty());
        }
        return queue;
    }

    size_t size() const
    {
        MutexLockGuard lock(mutex_);
        return queue_.size();
    }

private:
    mutable MutexLock mutex_;
    Condition         notEmpty_ GUARDED_BY(mutex_);
    queue_type        queue_ GUARDED_BY(mutex_);
} __attribute__ ((aligned (64)));;

}  // namespace muduo

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
