// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include "muduo/base/Mutex.h"
#include "muduo/base/noncopyable.h"

#include <pthread.h>

/**
 * @question why does need a key for the thread local object
 */
namespace muduo {

template<typename T>
class ThreadLocal : noncopyable {
public:
    ThreadLocal()
    {
        /**
         * @comment pthread_key_create
         * @brief
         * - prototype: int pthread_key_create(pthread_key_t *key, void (*destructor)(void*));
         * - purpose: The pthread_key_create() function shall create a thread-specific data key visible to all threads in the process.
         *            Key values provided by pthread_key_create() are opaque objects used to locate thread-specific data. Although the same key value may be used by
         *            different threads, the values bound to the key by pthread_setspecific() are maintained on a per-thread basis and persist for the life of the calling thread.
         */
        MCHECK(pthread_key_create(&pkey_, &ThreadLocal::destructor));
    }

    ~ThreadLocal()
    {
        /**
         * @comment pthread_key_delete
         * @brief
         * - prototype: int pthread_key_delete(pthread_key_t key);
         * - purpose: The pthread_key_delete() function shall delete a thread-specific data key previously returned by pthread_key_create().
         *            The thread-specific data values associated with key need not be NULL at the time pthread_key_delete() is called.
         *            It is the responsibility of the application to free any application storage or perform any cleanup actions for data structures related to the deleted key
         *            or associated thread-specific data in any threads; this cleanup can be done either before or after pthread_key_delete() is called.
         *            Any attempt to use key following the call to pthread_key_delete() results in undefined behavior.
         */
        MCHECK(pthread_key_delete(pkey_));
    }

    T& value()
    {
        /**
         * @comment pthread_getspecific
         * @brief
         * - prototype: void *pthread_getspecific(pthread_key_t key);
         * - purpose: The pthread_getspecific() function shall return the value currently bound to the specified key on behalf of the calling thread.
         */
        T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));
        if (!perThreadValue) {
            T* newObj = new T();
            /**
             * @comment pthread_setspecific
             * @brief
             * - prototype: int pthread_setspecific(pthread_key_t key, const void *value);
             * - purpose: The pthread_setspecific() function shall associate a thread-specific value with a key obtained via a previous call to pthread_key_create().
             *            Different threads may bind different values to the same key. These values are typically pointers to blocks of dynamically allocated memory
             *            that have been reserved for use by the calling thread.
             */
            MCHECK(pthread_setspecific(pkey_, newObj));
            perThreadValue = newObj;
        }
        return *perThreadValue;
    }

private:

    static void destructor(void *x)
    {
        T* obj = static_cast<T*>(x);
        /**
         * @comment  sizeof return 0
         * @link https://stackoverflow.com/questions/2632021/can-sizeof-return-0-zero
         * @brief
         * sizeof never returns 0 in C and in C++. Every time you see sizeof evaluating to 0 it is a bug/glitch/extension of a specific compiler that
         * has nothing to do with the language.
         */
        typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
        T_must_be_complete_type dummy;
        (void) dummy;
        delete obj;
    }

private:
    pthread_key_t pkey_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_THREADLOCAL_H
