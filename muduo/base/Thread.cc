// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Thread.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/Exception.h"
#include "muduo/base/Logging.h"

#include <type_traits>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

namespace muduo {
namespace detail {

pid_t gettid()
{
    /**
     * @comment syscall
     * @brief
     * - prototype: long syscall(long number, ...);
     * - purpose: syscall() is a small library function that invokes the system call whose assembly language interface has the specified number with
     *            the specified arguments.  Employing syscall() is useful, for example, when invoking a system call that has no wrapper function in the C library.
     */
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

void afterFork()
{
    muduo::CurrentThread::t_cachedTid = 0;
    muduo::CurrentThread::t_threadName = "main";
    CurrentThread::tid();
    // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

class ThreadNameInitializer {
public:
    ThreadNameInitializer()
    {
        muduo::CurrentThread::t_threadName = "main";
        CurrentThread::tid();
        /**
         * @comment pthread_atfork
         * @brief
         * - prototype: int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
         * - purpose:  The pthread_atfork() function registers fork handlers that are to be executed when fork(2) is called by this thread.
         *             The handlers are executed in the context of the thread that calls fork(2).
         *
         *             Three kinds of handler can be registered:
         *             - prepare specifies a handler that is executed before fork(2) processing starts.
         *             - parent specifies a handler that is executed in the parent process after fork(2) processing completes
         *             - child specifies a handler that is executed in the child process after fork(2) processing completes.
         */
        pthread_atfork(NULL, NULL, &afterFork);
    }
};

ThreadNameInitializer init;

struct ThreadData {
    typedef muduo::Thread::ThreadFunc ThreadFunc;
    ThreadFunc func_;
    string name_;
    pid_t* tid_;
    CountDownLatch* latch_;

    ThreadData(ThreadFunc func,
               const string& name,
               pid_t* tid,
               CountDownLatch* latch)
        : func_(std::move(func)),
          name_(name),
          tid_(tid),
          latch_(latch)
    { }

    void runInThread()
    {
        *tid_ = muduo::CurrentThread::tid();
        tid_ = NULL;
        latch_->countDown();
        latch_ = NULL;

        muduo::CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_.c_str();
        /**
         * @comment prctl, PR_SET_NAME
         * @brief
         * - prototype: int prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
         * - purpose: prctl() manipulates various aspects of the behavior of the calling thread or process.
         * - Set the name of the calling thread, using the value in the location pointed to by (char *) arg2.
         */
        ::prctl(PR_SET_NAME, muduo::CurrentThread::t_threadName);
        try {
            func_();
            muduo::CurrentThread::t_threadName = "finished";
        }
        catch (const Exception& ex) {
            muduo::CurrentThread::t_threadName = "crashed";
            fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
            fprintf(stderr, "reason: %s\n", ex.what());
            fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
            abort();
        }
        catch (const std::exception& ex) {
            muduo::CurrentThread::t_threadName = "crashed";
            fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
            fprintf(stderr, "reason: %s\n", ex.what());
            abort();
        }
        catch (...) {
            muduo::CurrentThread::t_threadName = "crashed";
            fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
            throw; // rethrow
        }
    }
};

void* startThread(void* obj)
{
    ThreadData* data = static_cast<ThreadData*>(obj);
    data->runInThread();
    delete data;
    return NULL;
}

}  // namespace detail

//!note: some CurrentThread functions are implemented here.
void CurrentThread::cacheTid()
{
    if (t_cachedTid == 0) {
        t_cachedTid = detail::gettid();
        t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
    }
}

bool CurrentThread::isMainThread()
{
    return tid() == ::getpid();
}

void CurrentThread::sleepUsec(int64_t usec)
{
    struct timespec ts = { 0, 0 };
    ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
    /**
     * @comment nanosleep
     * @brief 
     * - prototype: int nanosleep(const struct timespec *req, struct timespec *rem);
     * - purpose: nanosleep() suspends the execution of the calling thread until either at least the time specified in *req has elapsed, 
     *            or the delivery of a signal that triggers the invocation of a handler in the calling thread or that terminates the process.
     */
    ::nanosleep(&ts, NULL);
}

AtomicInt32 Thread::numCreated_;

Thread::Thread(ThreadFunc func, const string& n)
    : started_(false),
      joined_(false),
      pthreadId_(0),
      tid_(0),
      func_(std::move(func)),
      name_(n),
      latch_(1)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_) {
        /**
         * @comment pthread_detach
         * @brief
         * - prototype: int pthread_detach(pthread_t thread);
         * - purpose: The pthread_detach() function marks the thread identified by thread as detached.  When a detached thread terminates, 
         *            its resources are automatically released back to the system without the need for another thread to join with the terminated thread.
         */
        pthread_detach(pthreadId_);
    }
}

void Thread::setDefaultName()
{
    int num = numCreated_.incrementAndGet();
    if (name_.empty()) {
        char buf[32];
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}

void Thread::start()
{
    assert(!started_);
    started_ = true;
    // FIXME: move(func_)
    detail::ThreadData* data = new detail::ThreadData(func_, name_, &tid_, &latch_);
    /**
     * @comment pthread_create
     * @brief
     * - prototype: int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr, void *(*start_routine)(void *), void *restrict arg);
     * - purpose: The pthread_create() function starts a new thread in the calling process.  The new thread starts execution by invoking start_routine(); 
     *            arg is passed as the sole argument of start_routine().
     *
     *            The new thread terminates in one of the following ways:
     *
     *            - It calls pthread_exit(3), specifying an exit status value that is available to another thread in the same process that calls pthread_join(3)
     *            - It returns from start_routine().  This is equivalent to calling pthread_exit(3) with the value supplied in the return statement.
     *            - It is canceled (see pthread_cancel(3)).
     *            - Any of the threads in the process calls exit(3), or the main thread performs a return from main().  This causes the termination of all threads in the process.
     *
     *            The attr argument points to a pthread_attr_t structure whose contents are used at thread creation time to determine attributes for the new thread; 
     *            this structure is initialized using pthread_attr_init(3) and related functions.  If attr is NULL, then the thread is created with default attributes.
     */
    if (pthread_create(&pthreadId_, NULL, &detail::startThread, data)) {
        started_ = false;
        delete data; // or no delete?
        LOG_SYSFATAL << "Failed in pthread_create";
    }
    else {
        latch_.wait();
        assert(tid_ > 0);
    }
}

int Thread::join()
{
    assert(started_);
    assert(!joined_);
    joined_ = true;
    /**
     * @comment pthread_join
     * @brief
     * - prototype: int pthread_join(pthread_t thread, void **retval);
     * - purpose: The pthread_join() function waits for the thread specified by thread to terminate.  If that thread has already terminated, 
     *            then pthread_join() returns immediately.  The thread specified by thread must be joinable.
     */
    return pthread_join(pthreadId_, NULL);
}

}  // namespace muduo
