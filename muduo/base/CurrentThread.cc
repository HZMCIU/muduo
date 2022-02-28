// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/CurrentThread.h"

#include <cxxabi.h>
#include <execinfo.h>
#include <stdlib.h>

namespace muduo {
namespace CurrentThread {
__thread int t_cachedTid = 0;
__thread char t_tidString[32];
__thread int t_tidStringLength = 6;
__thread const char* t_threadName = "unknown";
static_assert(std::is_same<int, pid_t>::value, "pid_t should be int");

string stackTrace(bool demangle)
{
    string stack;
    const int max_frames = 200;
    void* frame[max_frames];
    /**
     * @comment backtrace
     * @brief
     * - prototype: int backtrace(void **buffer, int size);
     * - backtrace() returns a backtrace for the calling program, in the array pointed to by buffer.
     * - Each item in the array pointed to by buffer is of type void *, and is the return address from the corresponding stack frame.
     */
    int nptrs = ::backtrace(frame, max_frames);
    /**
     * @comment backtrace_symbols
     * @brief
     * - prototype: char **backtrace_symbols(void *const *buffer, int size);
     * - Given the set of addresses returned by backtrace() in buffer, backtrace_symbols() translates the addresses into an array of
     *   strings that describe the addresses symbolically.
     * - The symbolic representation of each address consists of the function name (if this can be determined), a hexadecimal offset into the
     *   function, and the actual return address (in hexadecimal).
     * - The address of the array of string pointers is returned as the function result of backtrace_symbols().  
     *   This array is malloc(3)ed by backtrace_symbols(), and must be freed by the caller.
     */
    char** strings = ::backtrace_symbols(frame, nptrs);
    if (strings) {
        size_t len = 256;
        char* demangled = demangle ? static_cast<char*>(::malloc(len)) : nullptr;
        for (int i = 1; i < nptrs; ++i) { // skipping the 0-th, which is this function
            if (demangle) {
                // https://panthema.net/2008/0901-stacktrace-demangled/
                // bin/exception_test(_ZN3Bar4testEv+0x79) [0x401909]
                char* left_par = nullptr;
                char* plus = nullptr;
                for (char* p = strings[i]; *p; ++p) {
                    if (*p == '(')
                        left_par = p;
                    else if (*p == '+')
                        plus = p;
                }

                if (left_par && plus) {
                    *plus = '\0';
                    int status = 0;
                    /**
                     * @comment __cxa_demangle
                     * @link https://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-4.3/a01696.html
                     * @brief 
                     * - prototype: char * __cxa_demangle (const char *mangled_name, char *output_buffer, size_t *length, int *status)
                     * - return: A pointer to the start of the NUL-terminated demangled name, or NULL if the demangling fails. 
                     *   The caller is responsible for deallocating this memory using free.
                     * - status:
                     *   - 0: The demangling operation succeeded.
                     *   - 1: A memory allocation failiure occurred.
                     *   - 2: mangled_name is not a valid name under the C++ ABI mangling rules.
                     *   - 3: One of the arguments is invalid.
                     */
                    char* ret = abi::__cxa_demangle(left_par + 1, demangled, &len, &status);
                    *plus = '+';
                    if (status == 0) {
                        demangled = ret;  // ret could be realloc()
                        stack.append(strings[i], left_par + 1);
                        stack.append(demangled);
                        stack.append(plus);
                        stack.push_back('\n');
                        continue;
                    }
                }
            }
            // Fallback to mangled names
            stack.append(strings[i]);
            stack.push_back('\n');
        }
        free(demangled);
        free(strings);
    }
    return stack;
}

}  // namespace CurrentThread
}  // namespace muduo
