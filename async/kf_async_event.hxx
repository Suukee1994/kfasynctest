#ifndef __KF_ASYNC__ASYNC_EVENT_H
#define __KF_ASYNC__ASYNC_EVENT_H

#include <base/kf_attr.hxx>
#include <async/kf_async_abstract.hxx>

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_ASYNC_EVENT "kf_iid_async_event"
#define _KF_INTERFACE_ID_ASYNC_EVENT_QUEUE "kf_iid_async_event_queue"
#else
#define _KF_INTERFACE_ID_ASYNC_EVENT "4131764C95C647FFB92A0178BF4F78A4"
#define _KF_INTERFACE_ID_ASYNC_EVENT_QUEUE "30F38DBD723F4AFDB859A53498927B85"
#endif 

#define KF_ASYNC_EVENT_TIMEOUT_INFINITE -1

struct IKFAsyncEvent : public IKFAttributes
{
    virtual KF_UINT32 GetEventType() = 0;
    virtual KF_RESULT GetEventResult() = 0;
    virtual KF_RESULT GetEventObject(IKFBaseObject** object) = 0;
};

KF_RESULT KFAPI KFCreateAsyncEvent(KF_UINT32 eventType, KF_RESULT eventResult, IKFBaseObject* eventObject, IKFAsyncEvent** asyncEvent);

template<typename QType>
inline KF_RESULT KFGetAsyncEventObject(IKFAsyncEvent* e, KIID iid, QType** object)
{
    IKFBaseObject* obj;
    auto result = e->GetEventObject(&obj);
    _KF_FAILED_RET(result);
    result = KFBaseGetInterface(obj, iid, object);
    obj->Recycle();
    return result;
}

inline bool KFEqualAsyncEventType(IKFAsyncEvent* e, KF_UINT32 eventType)
{ return e->GetEventType() == eventType; }
inline bool KFEqualAsyncEventResult(IKFAsyncEvent* e, KF_RESULT result)
{ return e->GetEventResult() == result; }

// ***************

struct IKFAsyncEventQueue : public IKFBaseObject
{
    virtual KF_RESULT Startup(KF_UINT32 flags = 0) = 0;
    virtual KF_RESULT Shutdown() = 0;

    virtual KF_RESULT GetEvent(IKFAsyncEvent** ppEvent, int timeout_ms) = 0;
    virtual KF_RESULT BeginGetEvent(IKFAsyncCallback* callback, IKFBaseObject* state) = 0;
    virtual KF_RESULT EndGetEvent(IKFAsyncEvent** ppEvent) = 0;

    virtual KF_RESULT QueueEvent(IKFAsyncEvent* pEvent) = 0;
    virtual KF_RESULT QueueEventDirect(KF_UINT32 eventType, KF_RESULT eventResult, IKFBaseObject* eventObject) = 0;
    virtual KF_RESULT QueueEventWithResult(KF_UINT32 eventType, KF_RESULT eventResult) = 0;
    virtual KF_RESULT QueueEventWithObject(KF_UINT32 eventType, IKFBaseObject* eventObject) = 0;

    virtual int GetPendingEventCount() = 0;
};

KF_RESULT KFAPI KFCreateAsyncEventQueue(IKFAsyncEventQueue** eventQueue);

#endif //__KF_ASYNC__ASYNC_EVENT_H