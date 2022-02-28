// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/LogStream.h"

#include <algorithm>
#include <limits>
#include <type_traits>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <inttypes.h>


using namespace muduo;
using namespace muduo::detail;

// TODO: better itoa.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wtautological-compare"
#else
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif

namespace muduo {
namespace detail {

const char digits[] = "9876543210123456789";
const char* zero = digits + 9;
static_assert(sizeof(digits) == 20, "wrong number of digits");

const char digitsHex[] = "0123456789ABCDEF";
static_assert(sizeof digitsHex == 17, "wrong number of digitsHex");

// Efficient Integer to String Conversions, by Matthew Wilson.
template<typename T>
size_t convert(char buf[], T value)
{
    T i = value;
    char* p = buf;

    do {
        int lsd = static_cast<int>(i % 10);
        i /= 10;
        *p++ = zero[lsd];
    }
    while (i != 0);

    if (value < 0) {
        *p++ = '-';
    }
    *p = '\0';
    std::reverse(buf, p);

    return p - buf;
}

size_t convertHex(char buf[], uintptr_t value)
{
    uintptr_t i = value;
    char* p = buf;

    do {
        int lsd = static_cast<int>(i % 16);
        i /= 16;
        *p++ = digitsHex[lsd];
    }
    while (i != 0);

    *p = '\0';
    std::reverse(buf, p);

    return p - buf;
}

template class FixedBuffer<kSmallBuffer>;
template class FixedBuffer<kLargeBuffer>;

}  // namespace detail

/*
 Format a number with 5 characters, including SI units.
 [0,     999]
 [1.00k, 999k]
 [1.00M, 999M]
 [1.00G, 999G]
 [1.00T, 999T]
 [1.00P, 999P]
 [1.00E, inf)
*/
std::string formatSI(int64_t s)
{
    /**
     * @comment digit format
     * @brief
     *      - format the digit so that only three digits will be displayed.
     *      - note: the digit after the last digit be displayed is 5, the rationale is prevent to rounding .
     */
    double n = static_cast<double>(s);
    char buf[64];
    if (s < 1000)
        /**
         * @comment snprintf
         * @brief
         * - prototype: int snprintf( char *restrict buffer, size_t bufsz, const char *restrict format, ... );
         * - purpose:  Writes the results to a character string buffer. At most bufsz - 1 characters are written.
         *             The resulting character string will be terminated with a null character, unless bufsz is zero.
         *             If bufsz is zero, nothing is written and buffer may be a null pointer,
         *             however the return value (number of bytes that would be written not including the null terminator) is still calculated and returned.
         */
        snprintf(buf, sizeof(buf), "%" PRId64, s);
    else if (s < 9995)
        snprintf(buf, sizeof(buf), "%.2fk", n / 1e3);
    else if (s < 99950)
        snprintf(buf, sizeof(buf), "%.1fk", n / 1e3);
    else if (s < 999500)
        snprintf(buf, sizeof(buf), "%.0fk", n / 1e3);
    else if (s < 9995000)
        snprintf(buf, sizeof(buf), "%.2fM", n / 1e6);
    else if (s < 99950000)
        snprintf(buf, sizeof(buf), "%.1fM", n / 1e6);
    else if (s < 999500000)
        snprintf(buf, sizeof(buf), "%.0fM", n / 1e6);
    else if (s < 9995000000)
        snprintf(buf, sizeof(buf), "%.2fG", n / 1e9);
    else if (s < 99950000000)
        snprintf(buf, sizeof(buf), "%.1fG", n / 1e9);
    else if (s < 999500000000)
        snprintf(buf, sizeof(buf), "%.0fG", n / 1e9);
    else if (s < 9995000000000)
        snprintf(buf, sizeof(buf), "%.2fT", n / 1e12);
    else if (s < 99950000000000)
        snprintf(buf, sizeof(buf), "%.1fT", n / 1e12);
    else if (s < 999500000000000)
        snprintf(buf, sizeof(buf), "%.0fT", n / 1e12);
    else if (s < 9995000000000000)
        snprintf(buf, sizeof(buf), "%.2fP", n / 1e15);
    else if (s < 99950000000000000)
        snprintf(buf, sizeof(buf), "%.1fP", n / 1e15);
    else if (s < 999500000000000000)
        snprintf(buf, sizeof(buf), "%.0fP", n / 1e15);
    else
        snprintf(buf, sizeof(buf), "%.2fE", n / 1e18);
    return buf;
}

/*
 [0, 1023]
 [1.00Ki, 9.99Ki]
 [10.0Ki, 99.9Ki]
 [ 100Ki, 1023Ki]
 [1.00Mi, 9.99Mi]
*/
std::string formatIEC(int64_t s)
{
    double n = static_cast<double>(s);
    char buf[64];
    const double Ki = 1024.0;
    const double Mi = Ki * 1024.0;
    const double Gi = Mi * 1024.0;
    const double Ti = Gi * 1024.0;
    const double Pi = Ti * 1024.0;
    const double Ei = Pi * 1024.0;

    if (n < Ki)
        snprintf(buf, sizeof buf, "%" PRId64, s);
    else if (n < Ki * 9.995)
        snprintf(buf, sizeof buf, "%.2fKi", n / Ki);
    else if (n < Ki * 99.95)
        snprintf(buf, sizeof buf, "%.1fKi", n / Ki);
    /**
     * @comment boundary
     * the boundary is 1024, so there is no 999.5
     */
    else if (n < Ki * 1023.5)
        snprintf(buf, sizeof buf, "%.0fKi", n / Ki);

    else if (n < Mi * 9.995)
        snprintf(buf, sizeof buf, "%.2fMi", n / Mi);
    else if (n < Mi * 99.95)
        snprintf(buf, sizeof buf, "%.1fMi", n / Mi);
    else if (n < Mi * 1023.5)
        snprintf(buf, sizeof buf, "%.0fMi", n / Mi);

    else if (n < Gi * 9.995)
        snprintf(buf, sizeof buf, "%.2fGi", n / Gi);
    else if (n < Gi * 99.95)
        snprintf(buf, sizeof buf, "%.1fGi", n / Gi);
    else if (n < Gi * 1023.5)
        snprintf(buf, sizeof buf, "%.0fGi", n / Gi);

    else if (n < Ti * 9.995)
        snprintf(buf, sizeof buf, "%.2fTi", n / Ti);
    else if (n < Ti * 99.95)
        snprintf(buf, sizeof buf, "%.1fTi", n / Ti);
    else if (n < Ti * 1023.5)
        snprintf(buf, sizeof buf, "%.0fTi", n / Ti);

    else if (n < Pi * 9.995)
        snprintf(buf, sizeof buf, "%.2fPi", n / Pi);
    else if (n < Pi * 99.95)
        snprintf(buf, sizeof buf, "%.1fPi", n / Pi);
    else if (n < Pi * 1023.5)
        snprintf(buf, sizeof buf, "%.0fPi", n / Pi);

    else if (n < Ei * 9.995)
        snprintf(buf, sizeof buf, "%.2fEi", n / Ei );
    else
        snprintf(buf, sizeof buf, "%.1fEi", n / Ei );
    return buf;
}

}  // namespace muduo

template<int SIZE>
const char* FixedBuffer<SIZE>::debugString()
{
    *cur_ = '\0';
    return data_;
}

template<int SIZE>
void FixedBuffer<SIZE>::cookieStart()
{
}

template<int SIZE>
void FixedBuffer<SIZE>::cookieEnd()
{
}

void LogStream::staticCheck()
{
    /**
     * @comment digits10
     * @link
     * - https://en.cppreference.com/w/cpp/types/numeric_limits/digits10
     * - https://stackoverflow.com/questions/747470/what-is-the-meaning-of-numeric-limitsdoubledigits10
     * @brief
     * - numeric_limits::digits10 is the number of decimal digits that can be held without loss.
     */
    /**
     * @question why minus 10?
     */
    static_assert(kMaxNumericSize - 10 > std::numeric_limits<double>::digits10,
                  "kMaxNumericSize is large enough");
    static_assert(kMaxNumericSize - 10 > std::numeric_limits<long double>::digits10,
                  "kMaxNumericSize is large enough");
    static_assert(kMaxNumericSize - 10 > std::numeric_limits<long>::digits10,
                  "kMaxNumericSize is large enough");
    static_assert(kMaxNumericSize - 10 > std::numeric_limits<long long>::digits10,
                  "kMaxNumericSize is large enough");
}

template<typename T>
void LogStream::formatInteger(T v)
{
    if (buffer_.avail() >= kMaxNumericSize) {
        size_t len = convert(buffer_.current(), v);
        buffer_.add(len);
    }
}

LogStream& LogStream::operator<<(short v)
{
    *this << static_cast<int>(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned short v)
{
    *this << static_cast<unsigned int>(v);
    return *this;
}

LogStream& LogStream::operator<<(int v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned int v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(long long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long long v)
{
    formatInteger(v);
    return *this;
}

LogStream& LogStream::operator<<(const void* p)
{
    /**
     * @comment reinterpret_cast
     * @link https://en.cppreference.com/w/cpp/language/reinterpret_cast
     * @brief
     * - Unlike static_cast, but like const_cast, the reinterpret_cast expression does not compile to any CPU instructions
     *   (except when converting between integers and pointers or on obscure architectures where pointer representation depends on its type).
     *   It is purely a compile-time directive which instructs the compiler to treat expression as if it had the type new-type.
     */
    /**
     * @comment uintptr_t
     * @link https://stackoverflow.com/questions/1845482/what-is-uintptr-t-data-type
     * @brief
     * In C99, it is defined as "an unsigned integer type with the property that any valid pointer to void can be converted to this type, 
     * then converted back to pointer to void, and the result will compare equal to the original pointer".
     */
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    if (buffer_.avail() >= kMaxNumericSize) {
        char* buf = buffer_.current();
        buf[0] = '0';
        buf[1] = 'x';
        size_t len = convertHex(buf + 2, v);
        buffer_.add(len + 2);
    }
    return *this;
}

// FIXME: replace this with Grisu3 by Florian Loitsch.
LogStream& LogStream::operator<<(double v)
{
    if (buffer_.avail() >= kMaxNumericSize) {
        int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", v);
        buffer_.add(len);
    }
    return *this;
}

template<typename T>
Fmt::Fmt(const char* fmt, T val)
{
    /**
     * @comment is_arithmetic
     * @link https://en.cppreference.com/w/cpp/types/is_arithmetic
     * @brief If T is an arithmetic type (that is, an integral type or a floating-point type) or a cv-qualified version thereof, 
     * provides the member constant value equal to true. For any other type, value is false.
     */
    static_assert(std::is_arithmetic<T>::value == true, "Must be arithmetic type");

    length_ = snprintf(buf_, sizeof buf_, fmt, val);
    assert(static_cast<size_t>(length_) < sizeof buf_);
}

// Explicit instantiations

template Fmt::Fmt(const char* fmt, char);

template Fmt::Fmt(const char* fmt, short);
template Fmt::Fmt(const char* fmt, unsigned short);
template Fmt::Fmt(const char* fmt, int);
template Fmt::Fmt(const char* fmt, unsigned int);
template Fmt::Fmt(const char* fmt, long);
template Fmt::Fmt(const char* fmt, unsigned long);
template Fmt::Fmt(const char* fmt, long long);
template Fmt::Fmt(const char* fmt, unsigned long long);

template Fmt::Fmt(const char* fmt, float);
template Fmt::Fmt(const char* fmt, double);
