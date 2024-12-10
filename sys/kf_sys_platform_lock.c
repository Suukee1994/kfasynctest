#include "kf_sys_platform.h"
#include <stdlib.h>
#ifndef _MSC_VER
#include <sys/time.h>
#include <errno.h>
#else
#pragma warning(disable:4100)
#endif

void* KF_SYS_CALL KFMutexCreate(int for_cond_var)
{
    void* result = NULL;
#ifdef _MSC_VER
    result = malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection((LPCRITICAL_SECTION)result);
#else
    result = malloc(sizeof(pthread_mutex_t));
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init((pthread_mutex_t*)result, for_cond_var ? NULL : &attr);
    pthread_mutexattr_destroy(&attr);
#endif
    return result;
}

void KF_SYS_CALL KFMutexDestroy(void* mutex)
{
#ifdef _MSC_VER
    DeleteCriticalSection((LPCRITICAL_SECTION)mutex);
#else
    pthread_mutex_destroy((pthread_mutex_t*)mutex);
#endif
    free(mutex);
}

void KF_SYS_CALL KFMutexLock(void* mutex)
{
#ifdef _MSC_VER
    EnterCriticalSection((LPCRITICAL_SECTION)mutex);
#else
    pthread_mutex_lock((pthread_mutex_t*)mutex);
#endif
}

void KF_SYS_CALL KFMutexUnlock(void* mutex)
{
#ifdef _MSC_VER
    LeaveCriticalSection((LPCRITICAL_SECTION)mutex);
#else
    pthread_mutex_unlock((pthread_mutex_t*)mutex);
#endif
}

//////////////////////////////////

void* KF_SYS_CALL KFRWLockCreate()
{
    void* result = NULL;
#ifdef _MSC_VER
    result = malloc(sizeof(SRWLOCK));
    InitializeSRWLock((PSRWLOCK)result);
#else
    result = malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init((pthread_rwlock_t*)result, NULL);
#endif
    return result;
}

void KF_SYS_CALL KFRWLockDestroy(void* lock)
{
#ifndef _MSC_VER
    pthread_rwlock_destroy((pthread_rwlock_t*)lock);
#endif
    free(lock);
}

void KF_SYS_CALL KFRWLockAcquireShared(void* lock)
{
#ifdef _MSC_VER
    AcquireSRWLockShared((PSRWLOCK)lock);
#else
    pthread_rwlock_rdlock((pthread_rwlock_t*)lock);
#endif
}

void KF_SYS_CALL KFRWLockAcquireExclusive(void* lock)
{
#ifdef _MSC_VER
    AcquireSRWLockExclusive((PSRWLOCK)lock);
#else
    pthread_rwlock_wrlock((pthread_rwlock_t*)lock);
#endif
}

void KF_SYS_CALL KFRWLockReleaseShared(void* lock)
{
#ifdef _MSC_VER
    ReleaseSRWLockShared((PSRWLOCK)lock);
#else
    pthread_rwlock_unlock((pthread_rwlock_t*)lock);
#endif
}

void KF_SYS_CALL KFRWLockReleaseExclusive(void* lock)
{
#ifdef _MSC_VER
    ReleaseSRWLockExclusive((PSRWLOCK)lock);
#else
    pthread_rwlock_unlock((pthread_rwlock_t*)lock);
#endif
}

//////////////////////////////////

void* KF_SYS_CALL KFCondVarCreate()
{
    void* result = NULL;
#ifdef _MSC_VER
    result = malloc(sizeof(CONDITION_VARIABLE));
    InitializeConditionVariable((PCONDITION_VARIABLE)result);
#else
    result = malloc(sizeof(pthread_cond_t));
    pthread_cond_init((pthread_cond_t*)result, NULL);
#endif
    return result;
}

void KF_SYS_CALL KFCondVarDestroy(void* cv)
{
#ifndef _MSC_VER
    pthread_cond_destroy((pthread_cond_t*)cv);
#endif
    free(cv);
}

void KF_SYS_CALL KFCondVarSignal(void* cv)
{
#ifdef _MSC_VER
    WakeConditionVariable((PCONDITION_VARIABLE)cv);
#else
    pthread_cond_signal((pthread_cond_t*)cv);
#endif
}

void KF_SYS_CALL KFCondVarBroadcast(void* cv)
{
#ifdef _MSC_VER
    WakeAllConditionVariable((PCONDITION_VARIABLE)cv);
#else
    pthread_cond_broadcast((pthread_cond_t*)cv);
#endif
}

void KF_SYS_CALL KFCondVarWait(void* cv, void* mutex)
{
#ifdef _MSC_VER
    SleepConditionVariableCS((PCONDITION_VARIABLE)cv, (PCRITICAL_SECTION)mutex, INFINITE);
#else
    pthread_cond_wait((pthread_cond_t*)cv, (pthread_mutex_t*)mutex);
#endif
}

int KF_SYS_CALL KFCondVarWaitTimed(void* cv, void* mutex, int time_out_ms)
{
#if _MSC_VER
    return SleepConditionVariableCS((PCONDITION_VARIABLE)cv,
        (PCRITICAL_SECTION)mutex,
        time_out_ms) ? KF_EVENT_COMPLETE : KF_EVENT_TIME_OUT;
#elif __APPLE__
    struct timespec time;
    time.tv_sec = time_out_ms / 1000;
    time.tv_nsec = (time_out_ms % 1000) * 1000000;
    int wait_result = pthread_cond_timedwait_relative_np((pthread_cond_t*)cv, (pthread_mutex_t*)mutex, &time);
#else
    struct timespec time;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time.tv_sec = tv.tv_sec + time_out_ms / 1000;
    time.tv_nsec = tv.tv_usec * 1000 + (time_out_ms % 1000) * 1000000;
    if (time.tv_nsec >= 1000000000) {
        time.tv_nsec -= 1000000000;
        time.tv_sec++;
    }
    int wait_result = pthread_cond_timedwait((pthread_cond_t*)cv, (pthread_mutex_t*)mutex, &time);
#endif
#ifndef _MSC_VER
    if (wait_result != 0 && wait_result != ETIMEDOUT)
        return -1;
    return wait_result == 0 ? KF_EVENT_COMPLETE : KF_EVENT_TIME_OUT;
#endif
}