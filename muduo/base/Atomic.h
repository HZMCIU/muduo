// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_ATOMIC_H
#define MUDUO_BASE_ATOMIC_H

#include "muduo/base/noncopyable.h"

#include <stdint.h>

namespace muduo {

namespace detail {
template<typename T>
class AtomicIntegerT : noncopyable {
public:
    AtomicIntegerT()
        : value_(0)
    {
    }

    // uncomment if you need copying and assignment
    //
    // AtomicIntegerT(const AtomicIntegerT& that)
    //   : value_(that.get())
    // {}
    //
    // AtomicIntegerT& operator=(const AtomicIntegerT& that)
    // {
    //   getAndSet(that.get());
    //   return *this;
    // }

    T get()
    {
        // in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
        /**
         * @comment 2 __sync_val_compare_and_swap
         * @link https://www.ibm.com/docs/en/xcfbg/121.141?topic=functions-sync-val-compare-swap
         * @brief 
         * - prototype: 'T __sync_val_compare_and_swap (T* __p, U __compVal, V __exchVal, ...);'
         * - purpose:
         *   - This function compares the value of __compVal to the value of the variable that __p points to. 
         *     If they are equal, the value of __exchVal is stored in the address that is specified by __p; otherwise, no operation is performed.
         *   - A full memory barrier is created when this function is invoked.
         */
        return __sync_val_compare_and_swap(&value_, 0, 0);
    }

    T getAndAdd(T x)
    {
        // in gcc >= 4.7: __atomic_fetch_add(&value_, x, __ATOMIC_SEQ_CST)
        /**
         * @comment 3 __sync_fetch_and_add
         * @link https://www.ibm.com/docs/en/xl-c-and-cpp-linux/16.1.0?topic=functions-sync-fetch-add
         * @brief
         * - prototype `T __sync_fetch_and_add (T* __p, U __v, ...); `
         * - purpose
         *   - This function atomically adds the value of __v to the variable that __p points to. The result is stored in the address that is specified by __p.
         *   - A full memory barrier is created when this function is invoked.
         * - return 
         *   - The function returns the initial value of the variable that __p points to.
         */
        return __sync_fetch_and_add(&value_, x);
    }

    T addAndGet(T x)
    {
        return getAndAdd(x) + x;
    }

    T incrementAndGet()
    {
        return addAndGet(1);
    }

    T decrementAndGet()
    {
        return addAndGet(-1);
    }

    void add(T x)
    {
        getAndAdd(x);
    }

    void increment()
    {
        incrementAndGet();
    }

    void decrement()
    {
        decrementAndGet();
    }

    T getAndSet(T newValue)
    {
        // in gcc >= 4.7: __atomic_exchange_n(&value_, newValue, __ATOMIC_SEQ_CST)
        /**
         * @comment 4 __sync_lock_test_and_set
         * @link https://www.ibm.com/docs/en/xcfbg/121.141?topic=functions-sync-lock-test-set
         * @brief
         * - prototype
         *   - T __sync_lock_test_and_set (T* __p, U __v, ...); 
         * - purpose
         *   - This function atomically assigns the value of __v to the variable that __p points to.
         *   - An acquire memory barrier is created when this function is invoked.
         * - return 
         *   - The function returns the initial value of the variable that __p points to.
         */
        return __sync_lock_test_and_set(&value_, newValue);
    }

private:
    /**
     * @comment 1 volatile key word.
     * @brief volatile is a hint to the implementation to avoid aggressive optimization involving the object because the value of the object might be changed 
     * by means undetectable by an implementation
     * @link https://stackoverflow.com/questions/4437527/why-do-we-use-volatile-keyword
     */
    volatile T value_;
};
}  // namespace detail

typedef detail::AtomicIntegerT<int32_t> AtomicInt32;
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;

}  // namespace muduo

#endif  // MUDUO_BASE_ATOMIC_H
