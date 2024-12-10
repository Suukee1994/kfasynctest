#include "kf_sys_platform.h"
#include <stdlib.h>
#include <memory.h>
#ifdef _MSC_VER
#include <objbase.h>
#endif
#ifdef __gnuc__
#include <sched.h>
#endif

static void all_platform_entry(KFThread* t)
{
    volatile int created_detach = t->detach;
    t->detach_state_on_thread_stack = (created_detach ? NULL : &created_detach);
    int result = t->func(t->data);
    if (!created_detach) {
        t->result = result;
        t->detach_state_on_thread_stack = NULL;
    }
}

#ifdef _MSC_VER
static DWORD CALLBACK Win32_ThreadProc(LPVOID data)
{
    KFThread* t = (KFThread*)data;
    EnterCriticalSection(&t->mutex);
    if (t->detach) {
        CloseHandle(t->thread);
        t->thread = NULL;
    }
    t->waked = 1;
    WakeConditionVariable(&t->cvar);
    LeaveCriticalSection(&t->mutex);

    CRITICAL_SECTION mutex = t->mutex;
    CoInitialize(NULL);
    all_platform_entry(t);
    CoUninitialize();
    DeleteCriticalSection(&mutex);
    return 0;
}
#else
static void* unix_thread_proc(void* data)
{
    KFThread* t = (KFThread*)data;
    pthread_mutex_lock(&t->mutex);
    if (t->name)
        pthread_setname_np(t->thread, t->name);
    if (t->detach)
        t->thread = NULL;
    t->waked = 1;
    pthread_cond_signal(&t->cvar);
    pthread_mutex_unlock(&t->mutex);
    
    pthread_mutex_t mutex = t->mutex;
    pthread_cond_t cvar = t->cvar;
    all_platform_entry(t);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cvar);
    return NULL;
}
#endif

int KF_SYS_CALL KFThreadCreate(KFThread* t, int (*fn)(void*), void* data, const char* name, int created_detach)
{
    memset(t, 0, sizeof(KFThread));
    t->func = fn;
    t->data = data;
    t->name = name;
    t->detach = created_detach;
#ifdef _MSC_VER
    InitializeCriticalSection(&t->mutex);
    InitializeConditionVariable(&t->cvar);
    t->thread = CreateThread(NULL, 0, &Win32_ThreadProc, t, 0, NULL);
    if (t->thread == NULL) {
        GetLastError();
        return 0;
    }

    EnterCriticalSection(&t->mutex);
    if (!t->waked)
        SleepConditionVariableCS(&t->cvar, &t->mutex, INFINITE);
    LeaveCriticalSection(&t->mutex);
    return 1;
#else
    pthread_mutex_init(&t->mutex, NULL);
    pthread_cond_init(&t->cvar, NULL);
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&t->thread, created_detach > 0 ? &attr : NULL, &unix_thread_proc, t) != 0) {
        pthread_attr_destroy(&attr);
        return 0;
    }
    pthread_attr_destroy(&attr);
    
    pthread_mutex_lock(&t->mutex);
    if (!t->waked)
        pthread_cond_wait(&t->cvar, &t->mutex);
    pthread_mutex_unlock(&t->mutex);
    return 1;
#endif
}

void KF_SYS_CALL KFThreadDetach(KFThread* t)
{
    if (t->thread) {
        if (t->detach_state_on_thread_stack) {
            *t->detach_state_on_thread_stack = 1;
            t->detach_state_on_thread_stack = NULL;
        }
    }

#ifdef _MSC_VER
    if (t->thread) {
        CloseHandle(t->thread);
        t->thread = NULL;
    }
#else
    if (t->thread) {
        pthread_detach(t->thread);
        t->thread = NULL;
    }
#endif
}

int KF_SYS_CALL KFThreadWait(KFThread* t)
{
    int result = KF_THREAD_INVALID_RESULT;
#ifdef _MSC_VER
    if (t->thread) {
        WaitForSingleObjectEx(t->thread, INFINITE, FALSE);
        CloseHandle(t->thread);
        t->thread = NULL;
        result = t->result;
    }
#else
    if (t->thread) {
        pthread_join(t->thread, NULL);
        t->thread = NULL;
        result = t->result;
    }
#endif
    return result;
}

void KF_SYS_CALL KFThreadInit(KFThread* t)
{
    memset(t, 0, sizeof(KFThread));
    t->result = KF_THREAD_INVALID_RESULT;
}

void* KF_SYS_CALL KFThreadPlatformObject(KFThread* t)
{
    if (t == NULL)
        return NULL;
    return t->thread;
}

void KF_SYS_CALL KFSwitchToThread()
{
#ifdef _MSC_VER
    SwitchToThread();
#elif __APPLE__
    pthread_yield_np();
#else
    sched_yield(); // pthread_yield();
#endif
}