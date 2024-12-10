#include "kf_sys_platform.h"
#ifndef _MSC_VER
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <memory.h>

void* KF_SYS_CALL KFEventCreate(int init_state, int manual_reset)
{
    KFEvent* e = (KFEvent*)malloc(sizeof(KFEvent));
    if (e == NULL)
        return NULL;
    
    e->state = init_state;
    e->manual_reset = manual_reset;
    if (pthread_mutex_init(&e->mutex, NULL)) {
        free(e);
        return NULL;
    }
    if (pthread_cond_init(&e->cond_var, NULL)) {
        pthread_mutex_destroy(&e->mutex);
        free(e);
        return NULL;
    }
    return e;
}

void KF_SYS_CALL KFEventDestroy(void* event)
{
    if (event == NULL)
        return;
    
    KFEvent* e = (KFEvent*)event;
    pthread_cond_destroy(&e->cond_var);
    pthread_mutex_destroy(&e->mutex);
    free(e);
}

int KF_SYS_CALL KFEventSet(void* event)
{
    if (event == NULL)
        return 0;
    
    KFEvent* e = (KFEvent*)event;
    pthread_mutex_lock(&e->mutex);
    e->state = 1;
    if (e->manual_reset)
        pthread_cond_broadcast(&e->cond_var);
    else
        pthread_cond_signal(&e->cond_var);
    pthread_mutex_unlock(&e->mutex);
    return 1;
}

int KF_SYS_CALL KFEventReset(void* event)
{
    if (event == NULL)
        return 0;
    
    KFEvent* e = (KFEvent*)event;
    pthread_mutex_lock(&e->mutex);
    e->state = 0;
    pthread_mutex_unlock(&e->mutex);
    return 1;
}

void KF_SYS_CALL KFEventWait(void* event)
{
    if (event == NULL)
        return;
    
    KFEvent* e = (KFEvent*)event;
    pthread_mutex_lock(&e->mutex);
    if (!e->state) {
        if (pthread_cond_wait(&e->cond_var, &e->mutex)) {
            pthread_mutex_unlock(&e->mutex);
            return;
        }
    }
    if (!e->manual_reset)
        e->state = 0;
    pthread_mutex_unlock(&e->mutex);
}

int KF_SYS_CALL KFEventWaitTimed(void* event, int time_out_ms)
{
    if (event == NULL)
        return -1;
    
    struct timespec time;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time.tv_sec = tv.tv_sec + time_out_ms / 1000;
    time.tv_nsec = tv.tv_usec * 1000 + (time_out_ms % 1000) * 1000000;
    if (time.tv_nsec >= 1000000000) {
        time.tv_nsec -= 1000000000;
        time.tv_sec++;
    }
    
    int wait_result = 0;
    KFEvent* e = (KFEvent*)event;
    pthread_mutex_lock(&e->mutex);
    if (!e->state) {
        wait_result = pthread_cond_timedwait(&e->cond_var, &e->mutex, &time);
        if (wait_result != 0 && wait_result != ETIMEDOUT) {
            pthread_mutex_unlock(&e->mutex);
            return -1;
        }
    }
    if (wait_result == 0 && !e->manual_reset)
        e->state = 0;
    pthread_mutex_unlock(&e->mutex);
    return wait_result == 0 ? KF_EVENT_COMPLETE : KF_EVENT_TIME_OUT;
}

#else

int KF_SYS_CALL KFEventWaitTimed(void* e, int time_out_ms)
{
    DWORD result = WaitForSingleObjectEx((HANDLE)e, time_out_ms, FALSE);
    return result == WAIT_OBJECT_0 ? KF_EVENT_COMPLETE : (result == WAIT_TIMEOUT ? KF_EVENT_TIME_OUT : -1);
}
#endif