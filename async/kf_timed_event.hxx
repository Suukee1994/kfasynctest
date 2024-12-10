#ifndef __KF_ASYNC__TIMED_EVENT_H
#define __KF_ASYNC__TIMED_EVENT_H

#include <base/kf_base.hxx>
#include <base/kf_attr.hxx>

typedef KF_UINT32 KF_TIMED_EVENT_ID;

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_TIMED_EVENT_STATE "kf_iid_timed_event_state"
#else
#define _KF_INTERFACE_ID_TIMED_EVENT_STATE "E4BE8685EE164F31B57F2D9AB27A24F1"
#endif
struct IKFTimedEventState : public IKFBaseObject
{
    virtual KF_RESULT GetAttributes(IKFAttributes** attr) = 0;
    virtual IKFAttributes* GetAttributesNoRef() = 0;

    virtual void SetObject(IKFBaseObject* object) = 0;
    virtual KF_RESULT GetObject(IKFBaseObject** object) = 0;
    virtual IKFBaseObject* GetObjectNoRef() = 0;
};

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_TIMED_EVENT_CALLBACK "kf_iid_timed_event_callback"
#else
#define _KF_INTERFACE_ID_TIMED_EVENT_CALLBACK "7F132407B6664A5AA4667B982381A0D5"
#endif
struct IKFTimedEventCallback : public IKFBaseObject
{
    virtual void Invoke(IKFTimedEventState* state, IKFBaseObject* queue) = 0;
};

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_TIMED_EVENT_QUEUE "kf_iid_timed_event_queue"
#else
#define _KF_INTERFACE_ID_TIMED_EVENT_QUEUE "035A1FBC9C364681B422A4E461DFE198"
#endif
struct IKFTimedEventQueue : public IKFBaseObject
{
    enum QueueShutdownFlushState
    {
        ExecuteTasks = 0,
        SkipTasks = 1
    };
    enum QueueShutdownAsyncMode
    {
        SyncShutdown = 0,
        AsyncShutdown = 1
    };

    virtual KF_RESULT Startup() = 0;
    virtual KF_RESULT Shutdown(QueueShutdownFlushState flush_state, QueueShutdownAsyncMode async_mode) = 0;

    virtual KF_RESULT PostEvent(IKFTimedEventState* callback, KF_TIMED_EVENT_ID* id) = 0;
    virtual KF_RESULT PostEventToBack(IKFTimedEventState* callback, KF_TIMED_EVENT_ID* id) = 0;
    virtual KF_RESULT PostEventWithDelay(IKFTimedEventState* callback, KF_INT32 delay_ms, KF_TIMED_EVENT_ID* id) = 0;

    virtual KF_RESULT CancelEvent(KF_TIMED_EVENT_ID id) = 0;
    virtual KF_RESULT CancelAllEvents() = 0;

    virtual int GetPendingEventCount() = 0;
};

// ***************

KF_RESULT KFAPI KFCreateTimedEventState(IKFTimedEventCallback* callback, IKFBaseObject* object, IKFTimedEventState** state);
KF_RESULT KFAPI KFCreateTimedEventQueue(IKFTimedEventQueue** queue);

KF_RESULT KFAPI KFPutTimedEventCallback(IKFTimedEventQueue* queue, IKFTimedEventCallback* callback, IKFBaseObject* object);
KF_RESULT KFAPI KFPutTimedEventCallbackAndCancelAllCancelAllEvents(IKFTimedEventQueue* queue, IKFTimedEventCallback* callback, IKFBaseObject* object);
KF_RESULT KFAPI KFPutTimedEventCallbackWithDelay(IKFTimedEventQueue* queue, IKFTimedEventCallback* callback, IKFBaseObject* object, KF_INT32 delay_ms);
KF_RESULT KFAPI KFPutTimedEventCallbackWithDelayAndCancelAllEvents(IKFTimedEventQueue* queue, IKFTimedEventCallback* callback, IKFBaseObject* object, KF_INT32 delay_ms);

inline KF_RESULT KFTimedEventQueueShutdownSync(IKFTimedEventQueue* queue)
{ return queue->Shutdown(IKFTimedEventQueue::QueueShutdownFlushState::SkipTasks, IKFTimedEventQueue::QueueShutdownAsyncMode::SyncShutdown); }
inline KF_RESULT KFTimedEventQueueShutdownAsync(IKFTimedEventQueue* queue)
{ return queue->Shutdown(IKFTimedEventQueue::QueueShutdownFlushState::SkipTasks, IKFTimedEventQueue::QueueShutdownAsyncMode::AsyncShutdown); }

#endif //__KF_ASYNC__TIMED_EVENT_H