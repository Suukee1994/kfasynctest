#include <utils/auto_mutex.hxx>
#include <async/kf_timed_event_internal.hxx>

class TimedEventState : public IKFTimedEventState_I
{
    KF_IMPL_DECL_REFCOUNT;

    struct State
    {
        IKFTimedEventCallback* callback;
        IKFBaseObject* object;
        KF_TIMED_EVENT_ID id;
        KF_INT64 time;

        State() throw()
        { id = TIMED_QUEUE_INVALID_EVENT_ID; time = 0; callback = nullptr; object = nullptr; }
        ~State() throw()
        { if (object) object->Recycle(); if (callback) callback->Recycle(); }
    };
    State _state;
    EventStateType _stateType;

    IKFAttributes* _attr;
    KFMutex* _mutex;

public:
    TimedEventState() throw() : _ref_count(1), _stateType(MethodInvoke), _attr(nullptr), _mutex(nullptr) {}
    virtual ~TimedEventState() throw() { if (_attr) _attr->Recycle(); if (_mutex) delete _mutex; }

    void SetThreadSafe(bool enable)
    {
        if (_mutex) {
            delete _mutex;
            _mutex = nullptr;
        }
        if (enable)
            _mutex = new(std::nothrow) KFMutex();
    }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_TIMED_EVENT_STATE) ||
            _KFInterfaceIdEqual(interface_id, _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE)) {
            *ppv = static_cast<IKFTimedEventState_I*>(this);
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
    virtual KF_RESULT GetAttributes(IKFAttributes** attr)
    {
        KFMutex::AutoLock lock(_mutex);
        if (_attr == nullptr) {
            auto r = KFCreateAttributes(&_attr);
            _KF_FAILED_RET(r);
        }
        _attr->Retain();
        *attr = _attr;
        return KF_OK;
    }
    
    
    virtual IKFAttributes* GetAttributesNoRef()
    {
        IKFAttributes* attr;
        GetAttributes(&attr);
        attr->Recycle();
        return _attr;
    }

    virtual void SetObject(IKFBaseObject* object)
    {
        KFMutex::AutoLock lock(_mutex);
        if (_state.object)
            _state.object->Recycle();

        if (object)
            object->Retain();
        _state.object = object;
    }

    virtual KF_RESULT GetObject(IKFBaseObject** object)
    {
        if (object == nullptr)
            return KF_INVALID_PTR;

        KFMutex::AutoLock lock(_mutex);
        if (_state.object == nullptr)
            return KF_NOT_FOUND;

        _state.object->Retain();
        *object = _state.object;
        return KF_OK;
    }

    virtual IKFBaseObject* GetObjectNoRef()
    {
        KFMutex::AutoLock lock(_mutex);
        return _state.object;
    }

public:
    virtual void SetTime(KF_INT64 time)
    {
        KFMutex::AutoLock lock(_mutex);
        _state.time = time;
    }
    virtual KF_INT64 GetTime()
    {
        KFMutex::AutoLock lock(_mutex);
        return _state.time;
    }

    virtual void SetEventId(KF_TIMED_EVENT_ID id)
    {
        KFMutex::AutoLock lock(_mutex);
        _state.id = id;
    }
    virtual KF_TIMED_EVENT_ID GetEventId()
    {
        KFMutex::AutoLock lock(_mutex);
        return _state.id;
    }

    virtual void SetCallback(IKFTimedEventCallback* callback)
    {
        KFMutex::AutoLock lock(_mutex);
        if (_state.callback)
            _state.callback->Recycle();
        _state.callback = callback;
        if (_state.callback)
            _state.callback->Retain();
    }
    virtual void GetCallback(IKFTimedEventCallback** callback)
    {
        KFMutex::AutoLock lock(_mutex);
        *callback = _state.callback;
        if (_state.callback)
            _state.callback->Retain();
    }

    virtual void SetEventType(EventStateType type)
    {
        KFMutex::AutoLock lock(_mutex);
        _stateType = type;
    }
    virtual EventStateType GetEventType()
    {
        KFMutex::AutoLock lock(_mutex);
        return _stateType;
    }
};

// ***************

KF_RESULT KFAPI KFCreateTimedEventState(IKFTimedEventCallback* callback, IKFBaseObject* object, IKFTimedEventState** state)
{
    if (state == nullptr)
        return KF_INVALID_PTR;
    
    auto s = new(std::nothrow) TimedEventState();
    if (s == nullptr)
        return KF_OUT_OF_MEMORY;

    s->SetThreadSafe(true);
    s->SetCallback(callback);
    s->SetObject(object);
    *state = static_cast<IKFTimedEventState*>(s);
    return KF_OK;
}

// ***************

KF_RESULT KFAPI KFPutTimedEventCallback(IKFTimedEventQueue* queue, IKFTimedEventCallback* callback, IKFBaseObject* object)
{
    if (queue == nullptr || callback == nullptr)
        return KF_INVALID_ARG;

    IKFTimedEventState* s = nullptr;
    auto result = KFCreateTimedEventState(callback, object, &s);
    _KF_FAILED_RET(result);

    result = queue->PostEvent(s, nullptr);
    s->Recycle();
    return result;
}

KF_RESULT KFAPI KFPutTimedEventCallbackWithDelay(IKFTimedEventQueue* queue, IKFTimedEventCallback* callback, IKFBaseObject* object, KF_INT32 delay_ms)
{
    if (queue == nullptr || callback == nullptr)
        return KF_INVALID_ARG;

    IKFTimedEventState* s = nullptr;
    auto result = KFCreateTimedEventState(callback, object, &s);
    _KF_FAILED_RET(result);

    result = queue->PostEventWithDelay(s, delay_ms, nullptr);
    s->Recycle();
    return result;
}

KF_RESULT KFAPI KFPutTimedEventCallbackAndCancelAllCancelAllEvents(IKFTimedEventQueue* queue, IKFTimedEventCallback* callback, IKFBaseObject* object)
{
    if (queue == nullptr)
        return KF_INVALID_ARG;

    auto result = queue->CancelAllEvents();
    _KF_FAILED_RET(result);
    result = KFPutTimedEventCallback(queue, callback, object);
    return result;
}

KF_RESULT KFAPI KFPutTimedEventCallbackWithDelayAndCancelAllEvents(IKFTimedEventQueue* queue, IKFTimedEventCallback* callback, IKFBaseObject* object, KF_INT32 delay_ms)
{
    if (queue == nullptr)
        return KF_INVALID_ARG;

    auto result = queue->CancelAllEvents();
    _KF_FAILED_RET(result);
    result = KFPutTimedEventCallbackWithDelay(queue, callback, object, delay_ms);
    return result;
}