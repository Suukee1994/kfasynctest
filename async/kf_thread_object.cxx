#include <async/kf_thread_object.hxx>
#include <base/kf_log.hxx>
#include <stdlib.h>

#define KF_LOG_TAG_STR "kf_thread_object.cxx"

struct ThreadParams
{
	KFThreadObject* Object;
	void* Userdata;
};

bool KFThreadObject::ThreadStart(void* param, bool joinable, const char* name) throw()
{
    ThreadParams* p = (ThreadParams*)malloc(sizeof(ThreadParams));
    if (p == nullptr) {
        KFLOG_ERROR_T("%s -> malloc error.", "KFThreadObject");
        return false;
    }

    _started_event = KFEventCreate(0, 1);
    if (_started_event == nullptr) {
        KFLOG_ERROR_T("%s -> KFEventCreate Failed!", "KFThreadObject");
        free(p);
        return false;
    }

    p->Object = this;
    p->Userdata = param;

    KFThreadInit(&_thread);
    if (!KFThreadCreate(&_thread, &KFThreadObject::ThreadEntry, p, name ? name : "KFThreadObject", joinable ? 0 : 1)) {
        KFLOG_ERROR_T("%s -> KFThreadCreate Failed!", "KFThreadObject");
        KFEventDestroy(_started_event);
        free(p);
        return false;
    }

    KFEventWait(_started_event);
    KFEventDestroy(_started_event);
    return true;
}

int KFThreadObject::DoThreadInvoke(void* param)
{
    KFEventSet(_started_event);
    OnThreadInvoke(param);
    return OnThreadExit();
}

int KFThreadObject::ThreadEntry(void* data)
{
    ThreadParams p = *(ThreadParams*)data;
    free(data);
    return ((KFThreadObject*)p.Object)->DoThreadInvoke(p.Userdata);
}