// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_BASE_PROCESSINFO_H
#define MUDUO_BASE_PROCESSINFO_H

#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include "muduo/base/Timestamp.h"
#include <vector>
#include <sys/types.h>

namespace muduo {

namespace ProcessInfo {
pid_t pid();
string pidString();
uid_t uid();
string username();
uid_t euid();
Timestamp startTime();
int clockTicksPerSecond();
int pageSize();
bool isDebugBuild();  // constexpr

string hostname();
string procname();
StringPiece procname(const string& stat);

/**
 * @comment process information file system
 * @brief
 * - The proc filesystem is a pseudo-filesystem which provides an interface to kernel data structures.  It is commonly mounted at /proc.  
 *   Typically, it is mounted automatically by the system, but it can also be mounted manually using a command such as:
 *      mount -t proc proc /proc
 *   Most of the files in the proc filesystem are read-only, but some files are writable, allowing kernel variables to be changed.
 * - Overview  Underneath /proc, there are the following general groups of files and subdirectories:
 *   - /proc/[pid] subdirectories
 *     Each one of these subdirectories contains files and subdirectories exposing information about the process with the corresponding process ID.
 *   - /proc/self
 *     When a process accesses this magic symbolic link, it resolves to the process's own /proc/[pid] directory.
 *   - /proc/[pid]/status
 *     Provides much of the information in /proc/[pid]/stat and /proc/[pid]/statm in a format that's easier for humans to parse.  Here's an example:
 *   - /proc/stat
 *     kernel/system statistics.  Varies with architecture. Common entries include:
 *   - /proc/[pid]/exe
 *     Under Linux 2.2 and later, this file is a symbolic link containing the actual pathname of the executed command.
 */
/// read /proc/self/status
string procStatus();

/// read /proc/self/stat
string procStat();

/// read /proc/self/task/tid/stat
string threadStat();

/// readlink /proc/self/exe
string exePath();

int openedFiles();
int maxOpenFiles();

struct CpuTime {
    double userSeconds;
    double systemSeconds;

    CpuTime() : userSeconds(0.0), systemSeconds(0.0) { }

    double total() const
    {
        return userSeconds + systemSeconds;
    }
};
CpuTime cpuTime();

int numThreads();
std::vector<pid_t> threads();
}  // namespace ProcessInfo

}  // namespace muduo

#endif  // MUDUO_BASE_PROCESSINFO_H
