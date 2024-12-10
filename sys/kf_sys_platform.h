#ifndef __KF_SYS_PLATFORM_H
#define __KF_SYS_PLATFORM_H

#ifdef _MSC_VER
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <pthread.h>
#endif

#ifndef KF_SYS_CALL
#ifdef _MSC_VER
#define KF_SYS_CALL __stdcall
#else
#define KF_SYS_CALL
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ***** Threads ***** //

#define KF_THREAD_INVALID_RESULT 0x7FFFFFFF

typedef struct {
#ifdef _MSC_VER
    HANDLE thread;
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE cvar;
#else
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cvar;
#endif
    int waked;
    int (*func)(void*);
    void* data;
    const char* name;
    int result;
    int detach;
    volatile int* detach_state_on_thread_stack;
} KFThread;
    
int   KF_SYS_CALL KFThreadCreate(KFThread* t, int (*fn)(void*), void* data, const char* name, int created_detach);
int   KF_SYS_CALL KFThreadWait(KFThread* t); //KFThreadDetach + WaitForExit.
void  KF_SYS_CALL KFThreadDetach(KFThread* t);
void  KF_SYS_CALL KFThreadInit(KFThread* t);
void* KF_SYS_CALL KFThreadPlatformObject(KFThread* t);

void  KF_SYS_CALL KFSwitchToThread();

// ***** Event ***** //

#define KF_EVENT_TIME_OUT 1
#define KF_EVENT_COMPLETE 0

#ifdef _MSC_VER
#define KFEventCreate(init_state, manual_reset) ((void*)CreateEventW(NULL, (manual_reset), (init_state), NULL))
#define KFEventDestroy(event) CloseHandle((event))
#define KFEventSet(event) ((int)SetEvent((event)))
#define KFEventReset(event) ((int)ResetEvent((event)))
#define KFEventWait(event) WaitForSingleObjectEx((event), INFINITE, FALSE)
#else
typedef struct {
    int state;
    int manual_reset;
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
} KFEvent;
    
void* KF_SYS_CALL KFEventCreate(int init_state, int manual_reset);
void  KF_SYS_CALL KFEventDestroy(void* event);
int   KF_SYS_CALL KFEventSet(void* event);
int   KF_SYS_CALL KFEventReset(void* event);
void  KF_SYS_CALL KFEventWait(void* event);
#endif
int   KF_SYS_CALL KFEventWaitTimed(void* event, int time_out_ms);

// ***** Mutex ***** //

void* KF_SYS_CALL KFMutexCreate(int for_cond_var);
void  KF_SYS_CALL KFMutexDestroy(void* mutex);
void  KF_SYS_CALL KFMutexLock(void* mutex);
void  KF_SYS_CALL KFMutexUnlock(void* mutex);

// ***** ReadWrite Lock ***** //

void* KF_SYS_CALL KFRWLockCreate();
void  KF_SYS_CALL KFRWLockDestroy(void* lock);
void  KF_SYS_CALL KFRWLockAcquireShared(void* lock);
void  KF_SYS_CALL KFRWLockAcquireExclusive(void* lock);
void  KF_SYS_CALL KFRWLockReleaseShared(void* lock);
void  KF_SYS_CALL KFRWLockReleaseExclusive(void* lock);

// ***** Condition Variable ***** //

void* KF_SYS_CALL KFCondVarCreate();
void  KF_SYS_CALL KFCondVarDestroy(void* cv);
void  KF_SYS_CALL KFCondVarSignal(void* cv);
void  KF_SYS_CALL KFCondVarBroadcast(void* cv);
void  KF_SYS_CALL KFCondVarWait(void* cv, void* mutex);
int   KF_SYS_CALL KFCondVarWaitTimed(void* cv, void* mutex, int time_out_ms);

// ***** Misc ***** //

int KF_SYS_CALL KFSystemCpuCount(void);

long long KF_SYS_CALL KFGetTick(void);
long long KF_SYS_CALL KFGetTime(void);
long long KF_SYS_CALL KFGetTimeTick(void);

void KF_SYS_CALL KFSleep(int sleep_ms);
#ifndef _MSC_VER
void KF_SYS_CALL KFSleepEx(int second, int ms);
#endif
unsigned int KF_SYS_CALL KFRandom32();
    
#ifdef __cplusplus
}
#endif

#endif //__KF_SYS_PLATFORM_H