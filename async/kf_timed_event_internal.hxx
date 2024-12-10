#ifndef __KF_ASYNC__TIMED_EVENT_INTERNAL_H
#define __KF_ASYNC__TIMED_EVENT_INTERNAL_H

#include <base/kf_fast_list.hxx>
#include <base/kf_attr.hxx>
#include <async/kf_timed_event.hxx>

#define TIMED_QUEUE_INVALID_EVENT_ID 0
#define TIMED_QUEUE_STARTUP_EVENT_ID 1

#ifndef KF_INTERFACE_ID_USE_GUID
#define _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE "__kf_iid_timed_event_state"
#else
#define _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE "_E4B1CEC02FB3492F9C76873509CBA757"
#endif
struct IKFTimedEventState_I : public IKFTimedEventState
{
    virtual void SetTime(KF_INT64 time) = 0;
    virtual KF_INT64 GetTime() = 0;

    virtual void SetEventId(KF_TIMED_EVENT_ID id) = 0;
    virtual KF_TIMED_EVENT_ID GetEventId() = 0;

    virtual void SetCallback(IKFTimedEventCallback* callback) = 0;
    virtual void GetCallback(IKFTimedEventCallback** callback) = 0;

    enum EventStateType
    {
        MethodInvoke = 0,
        QueueAbort
    };
    virtual void SetEventType(EventStateType type) = 0;
    virtual EventStateType GetEventType() = 0;
};

#ifndef KF_INTERFACE_ID_USE_GUID
#define _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_QUEUE "__kf_iid_timed_event_queue"
#else
#define _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_QUEUE "_69CE7438B7A64E52AF17BD4020564C13"
#endif
struct IKFTimedEventQueue_I : public IKFTimedEventQueue
{
    virtual void* GetThread() = 0;
    virtual IKFFastList* GetQueue() = 0;
    
    virtual bool IsStartup() = 0;
    virtual bool IsStopped() = 0;
};

#endif //__KF_ASYNC__TIMED_EVENT_INTERNAL_H