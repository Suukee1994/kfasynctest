#include <sys/kf_sys_platform.h>
#include <utils/auto_mutex.hxx>
#include <base/kf_queue.hxx>
#include <base/kf_log.hxx>
#include <async/kf_async_event.hxx>
#include <utils/async_callback_router.hxx>

#define KF_LOG_TAG_STR "kf_async_event_queue.cxx"

class AsyncEventQueue : public IKFAsyncEventQueue
{
    KF_IMPL_DECL_REFCOUNT;

    IKFQueue* _event_queue;
    void* _wake_queue_event;

    KASYNCOBJECT _exec_thread;
    AsyncCallbackRouter<AsyncEventQueue> _callback;
    bool _closed, _waiting;

    KFMutex _mutex;

public:
    AsyncEventQueue() throw() :
    _ref_count(1), _event_queue(nullptr), _wake_queue_event(nullptr), _exec_thread(nullptr)
    { _callback.SetCallback(this, &AsyncEventQueue::OnInvoke); }
    virtual ~AsyncEventQueue() throw()
    {
        if (_exec_thread) KFAsyncDestroyWorker(_exec_thread);
        if (_wake_queue_event) KFEventDestroy(_wake_queue_event);
        if (_event_queue) _event_queue->Recycle();
    }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_ASYNC_EVENT_QUEUE)) {
            *ppv = static_cast<IKFAsyncEventQueue*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }

    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }

public:
    virtual KF_RESULT Startup(KF_UINT32)
    {
        KFMutex::AutoLock lock(_mutex);

        if (_event_queue || _wake_queue_event) {
            KFLOG_ERROR_T("%s -> Startup -> Re-entry. (Failed)", "AsyncEventQueue");
            return KF_RE_ENTRY;
        }

        auto r = KFCreateObjectQueue(&_event_queue);
        if (KF_FAILED(r)) {
            KFLOG_ERROR_T("%s -> KFCreateObjectQueue Failed!", "AsyncEventQueue");
            return r;
        }

        _wake_queue_event = KFEventCreate(0, 0);
        if (_wake_queue_event == nullptr) {
            KFLOG_ERROR_T("%s -> KFEventCreate Failed.", "AsyncEventQueue");
            return KF_INIT_ERROR;
        }

        _closed = false;
        _waiting = false;
        return KFAsyncCreateWorker(false, 1, &_exec_thread);
    }

    virtual KF_RESULT Shutdown()
    {
        KFMutex::AutoLock lock(_mutex);
        if (_event_queue)
            _event_queue->Clear();
        _closed = true;

        if (_wake_queue_event) {
            KFLOG_T("%s -> Shutdown: Wake Queue Event to Exit.", "AsyncEventQueue");
            KFEventSet(_wake_queue_event);
        }
        return KF_OK;
    }

    virtual KF_RESULT GetEvent(IKFAsyncEvent** ppEvent, int timeout_ms)
    {
        if (ppEvent == nullptr)
            return KF_INVALID_PTR;
        
        bool to_wait = true;
        {
            KFMutex::AutoLock lock(_mutex);
            if (_event_queue == nullptr)
                return KF_NOT_INITIALIZED;

            if (_closed)
                return KF_SHUTDOWN;
            if (_event_queue->GetCount() > 0)
                to_wait = false;
        }
        KFLOG_T("%s -> GetEvent (wait?) : %s.", "AsyncEventQueue", to_wait ? "True" : "False");

        Retain(); //Hold KFEvent object!

        bool timeout = false;
        if (!to_wait) {
            KFEventReset(_wake_queue_event);
        }else{
            if (timeout_ms == KF_ASYNC_EVENT_TIMEOUT_INFINITE)
                KFEventWait(_wake_queue_event);
            else
                if (KFEventWaitTimed(_wake_queue_event, timeout_ms) == KF_EVENT_TIME_OUT)
                    timeout = true;
        }
        if (timeout) {
            KFLOG_T("%s -> GetEvent Timeout %d.", "AsyncEventQueue", timeout_ms);
            Recycle();
            return KF_TIMEOUT;
        }

        KFLOG_T("%s -> GetEvent wait waked!", "AsyncEventQueue");

        if (Recycle() == 0) { //Free hold object.
            KFLOG_T("%s -> GetEvent Self-Recycled!!!", "AsyncEventQueue");
            return KF_ABORT;
        }

        auto r = KF_OK;
        {
            KFMutex::AutoLock lock(_mutex);
            if (_closed)
                return KF_SHUTDOWN;
            if (_event_queue->GetCount() == 0)
                return KF_UNEXPECTED;
            
            IKFBaseObject* obj = nullptr;
            if (!_event_queue->Dequeue(&obj))
                return KF_ERROR;

            r = KFBaseGetInterface(obj, _KF_INTERFACE_ID_ASYNC_EVENT, ppEvent);
            obj->Recycle();
        }
        return r;
    }

    virtual KF_RESULT BeginGetEvent(IKFAsyncCallback* callback, IKFBaseObject* state)
    {
        if (callback == nullptr)
            return KF_INVALID_ARG;

        KFMutex::AutoLock lock(_mutex);
        if (_event_queue == nullptr)
            return KF_NOT_INITIALIZED;

        if (_closed) {
            KFLOG_T("%s -> BeginGetEvent Exception: KF_SHUTDOWN.", "AsyncEventQueue");
            return KF_SHUTDOWN;
        }
        if (_waiting) {
            KFLOG_T("%s -> BeginGetEvent Exception: KF_ABORT.", "AsyncEventQueue");
            return KF_ABORT; //必需调用EndGetEvent...
        }

        IKFAsyncResult* async_result = nullptr;
        auto r = KFAsyncCreateResult(&_callback, state, callback, &async_result);
        _KF_FAILED_RET(r);

        r = KFAsyncPutWorkItemEx(_exec_thread, async_result);
        async_result->Recycle();
        _KF_FAILED_RET(r);

        _waiting = true;
        return KF_OK;
    }

    virtual KF_RESULT EndGetEvent(IKFAsyncEvent** ppEvent)
    {
        if (ppEvent == nullptr)
            return KF_INVALID_PTR;

        KFMutex::AutoLock lock(_mutex);
        if (_event_queue == nullptr)
            return KF_NOT_INITIALIZED;

        if (_closed) {
            KFLOG_T("%s -> EndGetEvent Exception: KF_SHUTDOWN.", "AsyncEventQueue");
            return KF_SHUTDOWN;
        }
        if (!_waiting) {
            KFLOG_T("%s -> EndGetEvent Exception: KF_ABORT.", "AsyncEventQueue");
            return KF_ABORT;
        }

        if (_event_queue->GetCount() == 0) {
            KFLOG_T("%s -> EndGetEvent Error: EventQueue is Empty.", "AsyncEventQueue");
            return KF_UNEXPECTED;
        }

        IKFBaseObject* obj = nullptr;
        if (!_event_queue->Dequeue(&obj))
            return KF_ERROR;

        auto r = KFBaseGetInterface(obj, _KF_INTERFACE_ID_ASYNC_EVENT, ppEvent);
        obj->Recycle();
        
        _waiting = false;
        return r;
    }

    virtual KF_RESULT QueueEvent(IKFAsyncEvent* pEvent)
    {
        if (pEvent == nullptr)
            return KF_INVALID_ARG;

        KFMutex::AutoLock lock(_mutex);
        if (_event_queue == nullptr)
            return KF_NOT_INITIALIZED;
        if (_closed)
            return KF_SHUTDOWN;

        if (!_event_queue->Enqueue(pEvent))
            return KF_ABORT;

        KFLOG_T("%s -> QueueEvent: WAKE Event Queue... (Type: %d)", "AsyncEventQueue", pEvent->GetEventType());

        KFEventSet(_wake_queue_event);
        return KF_OK;
    }

    virtual KF_RESULT QueueEventDirect(KF_UINT32 eventType, KF_RESULT eventResult, IKFBaseObject* eventObject)
    {
        IKFAsyncEvent* event = nullptr;
        auto r = KFCreateAsyncEvent(eventType, eventResult, eventObject, &event);
        _KF_FAILED_RET(r);
        
        r = QueueEvent(event);
        event->Recycle();
        return r;
    }

    virtual KF_RESULT QueueEventWithResult(KF_UINT32 eventType, KF_RESULT eventResult)
    {
        IKFAsyncEvent* event = nullptr;
        auto r = KFCreateAsyncEvent(eventType, eventResult, nullptr, &event);
        _KF_FAILED_RET(r);
        
        r = QueueEvent(event);
        event->Recycle();
        return r;
    }

    virtual KF_RESULT QueueEventWithObject(KF_UINT32 eventType, IKFBaseObject* eventObject)
    {
        IKFAsyncEvent* event = nullptr;
        auto r = KFCreateAsyncEvent(eventType, KF_OK, eventObject, &event);
        _KF_FAILED_RET(r);

        r = QueueEvent(event);
        event->Recycle();
        return r;
    }

    virtual int GetPendingEventCount()
    {
        KFMutex::AutoLock lock(_mutex);
        if (_event_queue == nullptr)
            return 0;
        if (_closed)
            return 0;

        return _event_queue->GetCount();
    }

public:
    void OnInvoke(IKFAsyncResult* result)
    {
        bool to_wait = true;
        {
            KFMutex::AutoLock lock(_mutex);
            if (_event_queue->GetCount() > 0)
                to_wait = false;
        }
        KFLOG_T("%s -> AsyncEventQueue::OnInvoke (wait?) : %s.", "AsyncEventQueue", to_wait ? "True" : "False");

        if (to_wait)
            KFEventWait(_wake_queue_event);
        else
            KFEventReset(_wake_queue_event);

        KFLOG_T("%s -> AsyncEventQueue::OnInvoke wait waked!", "AsyncEventQueue");
        KF_RESULT event_result = KF_OK;
        {
            KFMutex::AutoLock lock(_mutex);
            if (_closed || _event_queue->GetCount() == 0)
                return;
            else
                event_result = ((IKFAsyncEvent*)_event_queue->Peek())->GetEventResult();
        }

        IKFAsyncCallback* callback = nullptr;
        if (KF_SUCCEEDED(KFBaseGetInterface(result->GetObjectNoRef(), _KF_INTERFACE_ID_ASYNC_CALLBACK, &callback))) {
            result->SetResult(event_result);
            callback->Execute(result);
            callback->Recycle();
        }
    }
};

// ***************

KF_RESULT KFAPI KFCreateAsyncEventQueue(IKFAsyncEventQueue** eventQueue)
{
    if (eventQueue == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) AsyncEventQueue();
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    *eventQueue = result;
    return KF_OK;
}