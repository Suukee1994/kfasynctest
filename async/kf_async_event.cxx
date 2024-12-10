#include <utils/auto_mutex.hxx>
#include <async/kf_async_event.hxx>

class AsyncEvent : public IKFAsyncEvent
{
    KF_IMPL_DECL_REFCOUNT;

    IKFAttributes* _attr;
    struct EventInfo
    {
        KF_UINT32 EventType;
        KF_RESULT EventResult;
        IKFBaseObject* EventObject;
    };
    EventInfo _info;

public:
    AsyncEvent() throw() : _ref_count(1) { memset(&_info, 0, sizeof(EventInfo)); }
    virtual ~AsyncEvent() throw()
    { if (_attr) _attr->Recycle(); if (_info.EventObject) _info.EventObject->Recycle(); }

    bool Prepare(KF_UINT32 eventType, KF_RESULT eventResult, IKFBaseObject* eventObject) throw()
    {
        if (KF_FAILED(KFCreateAttributesWithoutObserver(&_attr)))
            return false;

        if (eventObject)
            eventObject->Retain();

        _info.EventType = eventType;
        _info.EventResult = eventResult;
        _info.EventObject = eventObject;
        return true;
    }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_ASYNC_EVENT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_ATTRIBUTES)) {
            *ppv = static_cast<IKFAsyncEvent*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }

    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }

public: //IKFAttributes
    virtual KF_ATTRIBUTE_TYPE GetItemType(const char* key)
    { return _attr->GetItemType(key); }
    virtual KF_RESULT GetItemName(int index, IKFBuffer** name)
    { return _attr->GetItemName(index, name); }
    virtual int GetItemCount()
    { return _attr->GetItemCount(); }

    virtual KF_RESULT GetUINT32(const char* key, KF_UINT32* value)
    { return _attr->GetUINT32(key, value); }
    virtual KF_RESULT GetUINT64(const char* key, KF_UINT64* value)
    { return _attr->GetUINT64(key, value); }
    virtual KF_RESULT GetDouble(const char* key, double* value)
    { return _attr->GetDouble(key, value); }

    virtual KF_RESULT GetStringLength(const char* key, KF_UINT32* len)
    { return _attr->GetStringLength(key, len); }
    virtual KF_RESULT GetString(const char* key, char* string, KF_UINT32 str_ptr_len)
    { return _attr->GetString(key, string, str_ptr_len); }
    virtual KF_RESULT GetStringAlloc(const char* key, IKFBuffer** buffer)
    { return _attr->GetStringAlloc(key, buffer); }

    virtual KF_RESULT GetBlobLength(const char* key, KF_UINT32* len)
    { return _attr->GetBlobLength(key, len); }
    virtual KF_RESULT GetBlob(const char* key, void* buf, KF_UINT32 buf_ptr_len)
    { return _attr->GetBlob(key, buf, buf_ptr_len); }
    virtual KF_RESULT GetBlobAlloc(const char* key, IKFBuffer** buffer)
    { return _attr->GetBlobAlloc(key, buffer); }

    virtual KF_RESULT SetUINT32(const char* key, KF_UINT32 value)
    { return _attr->SetUINT32(key, value); }
    virtual KF_RESULT SetUINT64(const char* key, KF_UINT64 value)
    { return _attr->SetUINT64(key, value); }
    virtual KF_RESULT SetDouble(const char* key, double value)
    { return _attr->SetDouble(key, value); }

    virtual KF_RESULT SetString(const char* key, const char* value)
    { return _attr->SetString(key, value); }
    virtual KF_RESULT SetBlob(const char* key, const void* buf, KF_UINT32 buf_size)
    { return _attr->SetBlob(key, buf, buf_size); }

    virtual KF_RESULT GetObject(const char* key, KIID iid, void** ppv)
    { return _attr->GetObject(key, iid, ppv); }
    virtual KF_RESULT SetObject(const char* key, IKFBaseObject* object)
    { return _attr->SetObject(key, object); }

    virtual KF_RESULT DeleteItem(const char* key)
    { return _attr->DeleteItem(key); }
    virtual KF_RESULT DeleteAllItems()
    { return _attr->DeleteAllItems(); }

    virtual KF_RESULT MatchItem(const char* key, IKFAttributes* other_attr)
    { return _attr->MatchItem(key, other_attr); }
    virtual KF_RESULT MatchAllItems(IKFAttributes* other_attr)
    { return _attr->MatchAllItems(other_attr); }

    virtual KF_RESULT HasItem(const char* key)
    { return _attr->HasItem(key); }
    virtual KF_RESULT CopyItem(const char* key, IKFAttributes* copyTo)
    { return _attr->CopyItem(key, copyTo); }
    virtual KF_RESULT CopyAllItems(IKFAttributes* copyTo)
    { return _attr->CopyAllItems(copyTo); }

    virtual KF_RESULT SaveToBuffer(IKFBuffer** buffer)
    { return _attr->SaveToBuffer(buffer); }
    virtual KF_RESULT LoadFromBuffer(IKFBuffer* buffer)
    { return _attr->LoadFromBuffer(buffer); }

    virtual KF_RESULT ChangeObserverThreadMode(KF_ATTRIBUTE_OBSERVER_THREAD_MODE)
    { return KF_NOT_IMPLEMENTED; }

    virtual KF_RESULT AddObserver(IKFAttributesObserver*)
    { return KF_NOT_IMPLEMENTED; }
    virtual KF_RESULT RemoveObserver(IKFAttributesObserver*)
    { return KF_NOT_IMPLEMENTED; }

public: //IKFObjectReadWrite
    virtual int GetStreamLength()
    { return _attr->GetStreamLength(); }
    virtual KF_RESULT Read(KF_UINT8* buffer, int* length)
    { return _attr->Read(buffer, length); }
    virtual KF_RESULT Write(KF_UINT8* buffer, int length)
    { return _attr->Write(buffer, length); }

public: //IKFAsyncEvent
    virtual KF_UINT32 GetEventType() { return _info.EventType; }
    virtual KF_RESULT GetEventResult() { return _info.EventResult; }
    virtual KF_RESULT GetEventObject(IKFBaseObject** object)
    {
        if (object == nullptr)
            return KF_INVALID_PTR;
        *object = _info.EventObject;
        (*object)->Retain();
        return KF_OK;
    }
};

// ***************

KF_RESULT KFAPI KFCreateAsyncEvent(KF_UINT32 eventType, KF_RESULT eventResult, IKFBaseObject* eventObject, IKFAsyncEvent** asyncEvent)
{
    if (asyncEvent == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) AsyncEvent();
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    if (!result->Prepare(eventType, eventResult, eventObject)) {
        result->Recycle();
        return KF_INIT_ERROR;
    }

    *asyncEvent = result;
    return KF_OK;
}