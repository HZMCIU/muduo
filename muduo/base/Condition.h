// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include "muduo/base/Mutex.h"

#include <pthread.h>

namespace muduo {

class Condition : noncopyable {
public:
    explicit Condition(MutexLock& mutex)
        : mutex_(mutex)
    {
        /**
         * @comment pthread_cond_init
         * @brief
         * - prototype:  int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr);
         * - The pthread_cond_init() function shall initialize the condition variable referenced by cond with attributes referenced by attr.
         * - If attr is NULL, the default condition variable attributes shall be used; 
         *   the effect is the same as passing the address of a default condition variable attributes object.
         */
        MCHECK(pthread_cond_init(&pcond_, NULL));
    }

    ~Condition()
    {
        /**
         * @comment pthread_cond_destroy
         * @brief
         * - prototype: int pthread_cond_destroy(pthread_cond_t *cond);
         * - The pthread_cond_destroy() function shall destroy the given condition variable specified by cond; the object becomes, in effect, uninitialized.
         * - A destroyed condition variable object can be reinitialized using pthread_cond_init();
         */
        MCHECK(pthread_cond_destroy(&pcond_));
    }

    void wait()
    {
        MutexLock::UnassignGuard ug(mutex_);
        /**
         * @comment pthread_cond_wait
         * @link https://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_cond_wait.html
         * @brief
         * - prototype: int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
         * - pthread_cond_wait() functions shall block on a condition variable. They shall be called with mutex locked by the calling thread or undefined behavior results.
         * - These functions atomically release mutex and cause the calling thread to block on the condition variable cond
         * - Upon successful return, the mutex shall have been locked and shall be owned by the calling thread.
         */
        /**
         * @comment pthread_cond_wait usage
         * @link https://stackoverflow.com/questions/16522858/understanding-of-pthread-cond-wait-and-pthread-cond-signal
         * @ brief
         * pthread_cond_wait unlocks the mutex just before it sleeps (as you note), but then it reaquires the mutex (which may require waiting) when it is signalled, 
         * before it wakes up. So if the signalling thread holds the mutex (the usual case), the waiting thread will not proceed until the signalling thread also unlocks the mutex
         * thread 1:
         * pthread_mutex_lock(&mutex);
         * while (!condition)
         *     pthread_cond_wait(&cond, &mutex);
         * // do something that requires holding the mutex and condition is true 
         * pthread_mutex_unlock(&mutex);
         *
         * thread2:
         * pthread_mutex_lock(&mutex);
         * // do something that might make condition true 
         * pthread_cond_signal(&cond);
         * pthread_mutex_unlock(&mutex);
         */
        MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));
    }

    // returns true if time out, false otherwise.
    bool waitForSeconds(double seconds);

    void notify()
    {
        /**
         * @comment pthread_cond_signal
         * @link https://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_cond_signal.html
         * @brief
         * - prototype: int pthread_cond_signal(pthread_cond_t *cond);
         * - The pthread_cond_signal() call unblocks at least one of the threads that are blocked on the specified condition variable cond (if any threads are blocked on cond).
         * - return 
         *   - If successful, the pthread_cond_signal() and pthread_cond_broadcast() functions return zero. Otherwise, an error number is returned to indicate the error.
         */
        MCHECK(pthread_cond_signal(&pcond_));
    }

    void notifyAll()
    {
        /**
         * @comment pthread_cond_broadcast
         * @link  https://pubs.opengroup.org/onlinepubs/007904975/functions/pthread_cond_broadcast.html
         * @brief 
         * - prototype: int pthread_cond_broadcast(pthread_cond_t *cond);
         * - The pthread_cond_broadcast() function shall unblock all threads currently blocked on the specified condition variable cond.
         */
        MCHECK(pthread_cond_broadcast(&pcond_));
    }

private:
    MutexLock& mutex_;
    pthread_cond_t pcond_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_CONDITION_H
