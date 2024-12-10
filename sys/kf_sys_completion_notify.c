#include "kf_sys_completion_notify.h"
#include "kf_sys_platform.h"
#include <stdlib.h>

typedef struct {
    void* mutex;
    void* event;
    int fixedCount;
    int dynamicCount; //volatile
    int forceWaked;
} KFCompletionNotify;

void* KF_SYS_CALL KFCompletionNotifyCreate(int taskCount)
{
    if (taskCount == 0)
        return NULL;
    
    KFCompletionNotify notify = {0};
    notify.fixedCount = taskCount; //任务总数
    notify.dynamicCount = 0; //在运行中已完成的任务总数
    notify.event = KFEventCreate(0, 0); //完成所有任务后通知的event
    notify.mutex = KFMutexCreate(0); //保证多线程安全的lock
    if (notify.event == NULL ||
        notify.mutex == NULL) {
        if (notify.event)
            KFEventDestroy(notify.event);
        if (notify.mutex)
            KFMutexDestroy(notify.mutex);
        return NULL;
    }
    
    KFCompletionNotify* result = malloc(sizeof(KFCompletionNotify));
    if (result == NULL) {
        KFEventDestroy(notify.event);
        KFMutexDestroy(notify.mutex);
        return NULL;
    }
    
    *result = notify;
    return result;
}

void KF_SYS_CALL KFCompletionNotifyDestroy(void* notify)
{
    if (notify == NULL)
        return;
    
    KFCompletionNotify* core = (KFCompletionNotify*)notify;
    KFEventDestroy(core->event);
    KFMutexDestroy(core->mutex);
    free(notify);
}

int KF_SYS_CALL KFCompletionNotifyTryWake(void* notify)
{
    if (notify == NULL)
        return 0;
    
    int result = 0;
    
    KFCompletionNotify* core = (KFCompletionNotify*)notify;
    KFMutexLock(core->mutex); //锁定多线程，保证加法操作线程安全
    core->dynamicCount += 1; //完成一个任务+1
    if (core->dynamicCount == core->fixedCount) { //所有任务都已经完成
        KFEventSet(core->event); //通知事件，KFCompletionNotifyWait返回
        result = 1;
    }
    KFMutexUnlock(core->mutex);
    
    return result;
}

void KF_SYS_CALL KFCompletionNotifyForceWake(void* notify, int result)
{
    if (notify == NULL)
        return;
    
    KFCompletionNotify* core = (KFCompletionNotify*)notify;
    KFMutexLock(core->mutex);
    if (core->dynamicCount != core->fixedCount) { //只有在不相等的情况下
        core->dynamicCount = core->fixedCount;
        core->forceWaked = result;
        KFEventSet(core->event); //强制唤醒KFCompletionNotifyWait
    }
    KFMutexUnlock(core->mutex);
}

int KF_SYS_CALL KFCompletionNotifyWait(void* notify)
{
    if (notify == NULL)
        return 0;
    
    int result = 0;
    
    KFCompletionNotify* core = (KFCompletionNotify*)notify;
    KFMutexLock(core->mutex);
    if (core->dynamicCount == core->fixedCount) {
        //如果一进入wait方法就任务就已经全部完成，则reset后直接返回
        core->dynamicCount = 0;
        KFEventReset(core->event);
    }else{
        //不然则进入等待
        KFMutexUnlock(core->mutex);
        KFEventWait(core->event); //等待唤醒
        KFMutexLock(core->mutex);
        core->dynamicCount = 0; //reset任务完成计数
        if (core->forceWaked != 0) { //如果是强制wake的情况，拿返回值
            result = core->forceWaked;
            core->forceWaked = 0;
        }
    }
    KFMutexUnlock(core->mutex);
    
    return result;
}