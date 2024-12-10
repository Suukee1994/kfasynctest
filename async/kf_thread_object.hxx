#ifndef __KF_ASYNC__THREAD_OBJECT_H
#define __KF_ASYNC__THREAD_OBJECT_H

#include <sys/kf_sys_platform.h>

class KFThreadObject //继承这个类，实现单线程任务
{
protected:
    KFThreadObject() throw() : _started_event(nullptr) { KFThreadInit(&_thread); }
    ~KFThreadObject() throw()
    { if (KFThreadPlatformObject(&_thread)) KFThreadDetach(&_thread); }

    bool  ThreadStart(void* param = nullptr, bool joinable = true, const char* name = nullptr) throw();
    void* ThreadObject() throw() { return KFThreadPlatformObject(&_thread); }

    void  ThreadDetach() throw() { KFThreadDetach(&_thread); }
    int   ThreadJoin() throw() { return KFThreadWait(&_thread); }

protected:
    virtual void OnThreadInvoke(void* param) = 0;
    virtual int  OnThreadExit() { return 0; }

private:
    int DoThreadInvoke(void* param);

    KFThread _thread;
    void* _started_event;
    static int ThreadEntry(void* data);
};

#endif //__KF_ASYNC__THREAD_OBJECT_H