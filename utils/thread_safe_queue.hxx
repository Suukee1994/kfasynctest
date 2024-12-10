#ifndef __KF_UTIL__THREAD_SAFE_QUEUE_H
#define __KF_UTIL__THREAD_SAFE_QUEUE_H

#include <base/kf_ptr.hxx>
#include <base/kf_fast_list.hxx>
#include <utils/auto_mutex.hxx>

template<class T>
struct ThreadSafeQueue
{
    ThreadSafeQueue() throw()
    { KFCreateObjectFastList(&_list); }
    virtual ~ThreadSafeQueue() throw() {}

    KF_DISALLOW_COPY_AND_ASSIGN(ThreadSafeQueue)

    KF_RESULT Enqueue(T* object)
    {
        KFMutex::AutoLock lock(_mutex);
        return _list->PushBack(reinterpret_cast<IKFBaseObject*>(object)) ? KF_OK : KF_OUT_OF_MEMORY;
    }

    KF_RESULT Dequeue(T** object)
    {
        KFMutex::AutoLock lock(_mutex);
        if (_list->IsEmpty()) {
            *object = nullptr;
            return KF_PENDING;
        }
        _list->PopFront(reinterpret_cast<IKFBaseObject**>(object));
        return KF_OK;
    }

    KF_RESULT Requeue(T* object)
    {
        KFMutex::AutoLock lock(_mutex);
        return _list->PushFront(reinterpret_cast<IKFBaseObject*>(object)) ? KF_OK : KF_OUT_OF_MEMORY;
    }

    int Count() { KFMutex::AutoLock lock(_mutex); return _list->GetCount(); }
    void Clear() { KFMutex::AutoLock lock(_mutex); _list->Clear(); }

private:
    KFMutex _mutex;
    KFPtr<IKFFastList> _list;
};

#endif //__KF_UTIL__THREAD_SAFE_QUEUE_H