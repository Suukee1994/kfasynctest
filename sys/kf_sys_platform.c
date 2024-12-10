#include "kf_sys_platform.h"
#include <stdlib.h>
#ifndef _MSC_VER
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif
#ifdef _WINDOWS_
#include <NTSecAPI.h>
#define _RAND_WIN32
#endif
#ifdef __linux__
#if __GLIBC__ > 2 || __GLIBC_MINOR__ > 24
#include <sys/random.h>
#define _RAND_LINUX
#else
#include <sys/syscall.h>
#define _RAND_LINUX_SYSCALL
#endif
#endif

#ifdef _MSC_VER
static double kGlobalWin32QpcFrequency = 0;
LONG64 Win32GetTicks100ns()
{
    if (kGlobalWin32QpcFrequency == 0.0) {
        LONG64 frequency;
        QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
        kGlobalWin32QpcFrequency = (double)frequency;
    }
    if (kGlobalWin32QpcFrequency != 0.0) {
        LONG64 result;
        QueryPerformanceCounter((LARGE_INTEGER*)&result);
        result = (LONG64)(((double)result * 10000000.0) / kGlobalWin32QpcFrequency + 0.5);
        return result;
    }
    return 0;
}
#endif

#ifdef __APPLE__
static uint64_t kGlobalMachStartTime = 0;
static double kGlobalMachTimebase = 0;
uint64_t MachGetTicksNs()
{
    if (kGlobalMachStartTime == 0) {
        mach_timebase_info_data_t tb = {0};
        mach_timebase_info(&tb);
        kGlobalMachTimebase = (double)tb.numer / (double)tb.denom;
        kGlobalMachStartTime = mach_absolute_time();
    }
    if (kGlobalMachStartTime != 0) {
        double diff = (double)(mach_absolute_time() - kGlobalMachStartTime) * kGlobalMachTimebase;
        return (uint64_t)diff;
    }
    return 0;
}
#endif

int KF_SYS_CALL KFSystemCpuCount(void)
{
#ifdef _MSC_VER
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#elif __APPLE__
    int mib[2] = { CTL_HW, HW_NCPU };
    int nb_cpus = 1;
    size_t len = sizeof(nb_cpus);
    sysctl(mib, 2, &nb_cpus, &len, NULL, 0);
    return nb_cpus;
#else
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

long long KF_SYS_CALL KFGetTick(void) //ms
{
#ifdef _MSC_VER
    return Win32GetTicks100ns() / 10000;
#elif __APPLE__
    return MachGetTicksNs() / 1000000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

long long KF_SYS_CALL KFGetTime(void) //microsecond = ALooper::GetNowUs.
{
#ifdef _MSC_VER
    FILETIME ft;
    long long t;
    GetSystemTimeAsFileTime(&ft);
    t = (long long)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
    return t / 10 - 11644473600000000LL; /* Jan 1, 1601 */
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
#endif
}

long long KF_SYS_CALL KFGetTimeTick(void)
{
    return KFGetTime() / 1000;
}

void KF_SYS_CALL KFSleep(int sleep_ms)
{
#ifdef _MSC_VER
    SleepEx(sleep_ms, FALSE);
#else
    usleep(sleep_ms * 1000);
#endif
}

#ifndef _MSC_VER
void KF_SYS_CALL KFSleepEx(int second, int ms)
{
    struct timeval tv;
    tv.tv_sec = second;
    tv.tv_usec = ms * 1000;
    select(0, NULL, NULL, NULL, &tv);
}
#endif

unsigned int KF_SYS_CALL KFRandom32()
{
    unsigned int s = 0;
#ifdef _RAND_WIN32
    RtlGenRandom(&s, 4);
#elif defined(_RAND_LINUX)
    getrandom(&s, sizeof(unsigned int), 1);
#elif defined(_RAND_LINUX_SYSCALL)
    syscall(SYS_getrandom, &s, sizeof(unsigned int), 1);
#else
    srand((unsigned int)KFGetTick());
    s = rand();
#endif
    return s;
}