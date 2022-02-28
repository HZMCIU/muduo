// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/ProcessInfo.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/FileUtil.h"

#include <algorithm>

#include <assert.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h> // snprintf
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/times.h>

namespace muduo {
namespace detail {
__thread int t_numOpenedFiles = 0;
/**
 * @comment struct dirent
 * @link
 * - https://stackoverflow.com/questions/12991334/members-of-dirent-structure
 * - https://www.gnu.org/software/libc/manual/html_node/Directory-Entries.html
 * @brief
 * - This section describes what you find in a single directory entry, as you might obtain it from a directory stream.
 * -
 *   ```cpp
 *   struct dirent {
 *      ino_t          d_ino;       // inode number
 *      off_t          d_off;       // offset to the next dirent
 *      unsigned short d_reclen;    // length of this record
 *      unsigned char  d_type;      // type of file; not supported  by all file system types
 *      char           d_name[256]; // filename
 *   };
 *   ```
 */
int fdDirFilter(const struct dirent* d)
{
    if (::isdigit(d->d_name[0])) {
        ++t_numOpenedFiles;
    }
    return 0;
}

__thread std::vector<pid_t>* t_pids = NULL;
int taskDirFilter(const struct dirent* d)
{
    if (::isdigit(d->d_name[0])) {
        /**
         * @comment atoi
         * @link https://en.cppreference.com/w/c/string/byte/atoi
         * @brief
         * - prototype: int       atoi( const char *str );
         * - purpose: Interprets an integer value in a byte string pointed to by str.
         */
        t_pids->push_back(atoi(d->d_name));
    }
    return 0;
}

int scanDir(const char *dirpath, int (*filter)(const struct dirent *))
{
    struct dirent** namelist = NULL;
    /**
     * @comment scandir
     * @brief
     * - prototype: int scandir(const char *restrict dirp,
     *                          struct dirent ***restrict namelist,
     *                          int (*filter)(const struct dirent *),
     *                          int (*compar)(const struct dirent **,
     *                          const struct dirent **));
     * - purpose: The scandir() function scans the directory dirp, calling filter() on each directory entry.
     *            Entries for which filter() returns nonzero are stored in strings allocated via malloc(3),
     *            sorted using qsort(3) with the comparison function compar(), and collected in array namelist which is allocated via malloc(3).
     *            If filter is NULL, all entries are selected.
     * - return:
     *   - The scandir() function returns the number of directory entries selected.  On error, -1 is returned, with errno set to indicate the error.
     */

    /**
     * @comment alphasort
     * @brief
     * - prototype: int alphasort(const struct dirent **a, const struct dirent **b);
     * - purpose: The alphasort() and versionsort() functions can be used as the comparison function compar().
     *            The former sorts directory entries using strcoll(3), the latter using strverscmp(3) on the strings (*a)->d_name and (*b)->d_name.
     */

    /**
     * @comment strcoll
     * @brief
     * - prototype: int strcoll(const char *s1, const char *s2);
     * - purpose: The strcoll() function compares the two strings s1 and s2.  It returns an integer less than, equal to, or greater than zero if s1 is found,
     *            respectively, to be less than, to match, or be greater than s2.
     *            The comparison is based on strings interpreted as appropriate for the program's current locale for category LC_COLLATE.
     */

    int result = ::scandir(dirpath, &namelist, filter, alphasort);
    assert(namelist == NULL);
    return result;
}

Timestamp g_startTime = Timestamp::now();
// assume those won't change during the life time of a process.
/**
 * @comment sysconf
 * @brief
 * - prototype: long sysconf(int name);
 * - purpose: POSIX allows an application to test at compile or run time whether certain options are supported,
 *            or what the value is of certain configurable constants or limits.
 * - macros:
 *   - _SC_CLK_TCK: The number of clock ticks per second.  The corresponding variable is obsolete.
 *   - _SC_PAGESIZE: Size of a page in bytes.  Must not be less than 1.
 */
int g_clockTicks = static_cast<int>(::sysconf(_SC_CLK_TCK));
int g_pageSize = static_cast<int>(::sysconf(_SC_PAGE_SIZE));
}  // namespace detail
}  // namespace muduo

using namespace muduo;
using namespace muduo::detail;

pid_t ProcessInfo::pid()
{
    /**
     * @comment getpid
     * @brief
     * - prototype: pid_t getpid(void);
     * - purpose: getpid() returns the process ID (PID) of the calling process. (This is often used by routines that generate unique temporary filenames.)
     */
    return ::getpid();
}

string ProcessInfo::pidString()
{
    char buf[32];
    snprintf(buf, sizeof buf, "%d", pid());
    return buf;
}

uid_t ProcessInfo::uid()
{
    /**
     * @comment getuid
     * @brief
     * - prototype: uid_t getuid(void);
     * - purpose: getuid() returns the real user ID of the calling process.
     */
    return ::getuid();
}

string ProcessInfo::username()
{
    /**
     * @comment struct passwd
     * @brief
     * The passwd structure is defined in <pwd.h> as follows:
     * struct passwd {
     *     char   *pw_name;       // username
     *     char   *pw_passwd;     // user password
     *     uid_t   pw_uid;        // user ID
     *     gid_t   pw_gid;        // group ID
     *     char   *pw_gecos;      // user information
     *     char   *pw_dir;        // home directory
     *     char   *pw_shell;      // shell program
     * };
     */
    struct passwd pwd;
    struct passwd* result = NULL;
    char buf[8192];
    const char* name = "unknownuser";

    /**
     * @comment getpwuid_r
     * @brief
     * - prototype: 
     *   - int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result);
     *   - struct passwd *getpwuid(uid_t uid);
     * - purpose: 
     *   - The getpwuid() function returns a pointer to a structure containing the broken-out fields of the record in the password database that matches the user ID uid.
     *   - The getpwnam_r() and getpwuid_r() functions obtain the same information as getpwnam() and getpwuid(), 
     *     but store the retrieved passwd structure in the space pointed to by pwd. The string fields pointed to by the members of the passwd structure 
     *     are stored in the buffer buf of size buflen. A pointer to the result (in case of success) or NULL (in case no entry was found or an error occurred) 
     *     is stored in *result.
     */
    getpwuid_r(uid(), &pwd, buf, sizeof buf, &result);
    if (result) {
        name = pwd.pw_name;
    }
    return name;
}

uid_t ProcessInfo::euid()
{
    /**
     * @comment geteuid
     * @brief
     * - prototype: uid_t geteuid(void);
     * - purpose: The geteuid() function shall return the effective user ID of the calling process.  The geteuid() function shall not modify errno.
     */
    return ::geteuid();
}

Timestamp ProcessInfo::startTime()
{
    return g_startTime;
}

int ProcessInfo::clockTicksPerSecond()
{
    return g_clockTicks;
}

int ProcessInfo::pageSize()
{
    return g_pageSize;
}

bool ProcessInfo::isDebugBuild()
{
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

string ProcessInfo::hostname()
{
    // HOST_NAME_MAX 64
    // _POSIX_HOST_NAME_MAX 255
    char buf[256];
    /**
     * @comment gethostname
     * @brief
     * - prototype:  int gethostname(char *name, size_t len);
     * - purpose: gethostname() returns the null-terminated hostname in the character array name, which has a length of len bytes.  
     *   If the null-terminated hostname is too large to fit, then the name is truncated, and no error is returned
     */
    if (::gethostname(buf, sizeof buf) == 0) {
        buf[sizeof(buf) - 1] = '\0';
        return buf;
    }
    else {
        return "unknownhost";
    }
}

string ProcessInfo::procname()
{
    return procname(procStat()).as_string();
}

StringPiece ProcessInfo::procname(const string& stat)
{
    StringPiece name;
    size_t lp = stat.find('(');
    size_t rp = stat.rfind(')');
    if (lp != string::npos && rp != string::npos && lp < rp) {
        name.set(stat.data() + lp + 1, static_cast<int>(rp - lp - 1));
    }
    return name;
}

string ProcessInfo::procStatus()
{
    string result;
    FileUtil::readFile("/proc/self/status", 65536, &result);
    return result;
}

string ProcessInfo::procStat()
{
    string result;
    FileUtil::readFile("/proc/self/stat", 65536, &result);
    return result;
}

string ProcessInfo::threadStat()
{
    char buf[64];
    snprintf(buf, sizeof buf, "/proc/self/task/%d/stat", CurrentThread::tid());
    string result;
    FileUtil::readFile(buf, 65536, &result);
    return result;
}

string ProcessInfo::exePath()
{
    string result;
    char buf[1024];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf);
    if (n > 0) {
        result.assign(buf, n);
    }
    return result;
}

int ProcessInfo::openedFiles()
{
    t_numOpenedFiles = 0;
    scanDir("/proc/self/fd", fdDirFilter);
    return t_numOpenedFiles;
}

int ProcessInfo::maxOpenFiles()
{
    struct rlimit rl;
    /**
     * @comment getrlimit
     * @brief
     * - prototype: int getrlimit(int resource, struct rlimit *rlim);
     * - purpose: 
     *   - The getrlimit() and setrlimit() system calls get and set resource limits.  Each resource has an associated soft and hard limit, as defined by the rlimit structure:
     *     ```cpp
     *     struct rlimit {
     *         rlim_t rlim_cur;  // Soft limit 
     *         rlim_t rlim_max;  // Hard limit (ceiling for rlim_cur)
     *     };
     *     ```
     *   - The soft limit is the value that the kernel enforces for the corresponding resource.  
     *     The hard limit acts as a ceiling for the soft limit: an unprivileged process may set only its soft limit to a value in the range from 0 up to the hard limit, 
     *     and (irreversibly) lower its hard limit.  A privileged process (under Linux: one with the CAP_SYS_RESOURCE capability in the initial user namespace) 
     *     may make arbitrary changes to either limit value.
     *   - RLIMIT_NOFILE:
     *      - This specifies a value one greater than the maximum file descriptor number that can be opened by this process.
     */
    if (::getrlimit(RLIMIT_NOFILE, &rl)) {
        return openedFiles();
    }
    else {
        return static_cast<int>(rl.rlim_cur);
    }
}

ProcessInfo::CpuTime ProcessInfo::cpuTime()
{
    ProcessInfo::CpuTime t;
    struct tms tms;
    if (::times(&tms) >= 0) {
        const double hz = static_cast<double>(clockTicksPerSecond());
        t.userSeconds = static_cast<double>(tms.tms_utime) / hz;
        t.systemSeconds = static_cast<double>(tms.tms_stime) / hz;
    }
    return t;
}

int ProcessInfo::numThreads()
{
    int result = 0;
    string status = procStatus();
    size_t pos = status.find("Threads:");
    if (pos != string::npos) {
        result = ::atoi(status.c_str() + pos + 8);
    }
    return result;
}

std::vector<pid_t> ProcessInfo::threads()
{
    std::vector<pid_t> result;
    t_pids = &result;
    scanDir("/proc/self/task", taskDirFilter);
    t_pids = NULL;
    std::sort(result.begin(), result.end());
    return result;
}
