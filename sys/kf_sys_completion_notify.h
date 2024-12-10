#ifndef __KF_SYS_COMPLETION_NOTIFY_H
#define __KF_SYS_COMPLETION_NOTIFY_H

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

void* KF_SYS_CALL KFCompletionNotifyCreate(int taskCount);
void  KF_SYS_CALL KFCompletionNotifyDestroy(void* notify);
int   KF_SYS_CALL KFCompletionNotifyTryWake(void* notify);
void  KF_SYS_CALL KFCompletionNotifyForceWake(void* notify, int result);
int   KF_SYS_CALL KFCompletionNotifyWait(void* notify);
    
#ifdef __cplusplus
}
#endif

#endif //__KF_SYS_COMPLETION_NOTIFY_H