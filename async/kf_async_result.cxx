#include <utils/auto_mutex.hxx>
#include <base/kf_base.hxx>
#include <async/kf_async_internal.hxx>

class AsyncResult : public IKFAsyncResult_I
{
    KF_IMPL_DECL_REFCOUNT;

    IKFAsyncCallback* _callback;

    KF_RESULT _result;
    IKFBaseObject* _object;
    IKFBaseObject* _state;

    KFMutex _mutex;

public:
    AsyncResult(KF_RESULT result, IKFBaseObject* object, IKFBaseObject* state) throw() :
    _ref_count(1), _result(result), _object(object), _state(state), _callback(nullptr)
    { if (_object) _object->Retain(); if (_state) _state->Retain(); }
    virtual ~AsyncResult() throw()
    { if (_object) _object->Recycle(); if (_state) _state->Recycle(); if (_callback) _callback->Recycle(); }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_ASYNC_RESULT) ||
            _KFInterfaceIdEqual(interface_id, _INTERNAL_KF_INTERFACE_ID_ASYNC_RESULT)) {
            *ppv = static_cast<IKFAsyncResult_I*>(this);
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
    virtual void SetResult(KF_RESULT result)
    {
        KFMutex::AutoLock lock(_mutex);
        _result = result;
    }

    virtual void SetObject(IKFBaseObject* pObject)
    {
        KFMutex::AutoLock lock(_mutex);
        if (_object != nullptr) {
            _object->Recycle();
            _object = nullptr;
        }

        if (pObject != nullptr) {
            pObject->Retain();
            _object = pObject;
        }
    }

    virtual KF_RESULT GetResult()
    {
        KFMutex::AutoLock lock(_mutex);
        return _result;
    }

    virtual KF_RESULT GetState(IKFBaseObject** ppState)
    {
        if (ppState == nullptr)
            return KF_INVALID_PTR;

        KFMutex::AutoLock lock(_mutex);
        if (_state == nullptr)
            return KF_NOT_FOUND;

        _state->Retain();
        *ppState = _state;
        return KF_OK;
    }

    virtual KF_RESULT GetObject(IKFBaseObject** ppObject)
    {
        if (ppObject == nullptr)
            return KF_INVALID_PTR;

        KFMutex::AutoLock lock(_mutex);
        if (_object == nullptr)
            return KF_NOT_FOUND;

        _object->Retain();
        *ppObject = _object;
        return KF_OK;
    }

    virtual IKFBaseObject* GetStateNoRef()
    {
        KFMutex::AutoLock lock(_mutex);
        return _state;
    }

    virtual IKFBaseObject* GetObjectNoRef()
    {
        KFMutex::AutoLock lock(_mutex);
        return _object;
    }

public:
    virtual void SetCallback(IKFAsyncCallback* callback)
    {
        KFMutex::AutoLock lock(_mutex);
        if (_callback)
            _callback->Recycle();
        _callback = callback;
        _callback->Retain();
    }

    virtual void GetCallback(IKFAsyncCallback** callback)
    {
        KFMutex::AutoLock lock(_mutex);
        *callback = _callback;
        _callback->Retain();
    }
};

// ***************

KF_RESULT KFAsyncCreateResult(IKFAsyncCallback* callback, IKFBaseObject* state, IKFBaseObject* object, IKFAsyncResult** asyncResult)
{
    if (callback == nullptr)
        return KF_INVALID_ARG;
    if (asyncResult == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) AsyncResult(KF_OK, object, state);
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    result->SetCallback(callback);
    *asyncResult = static_cast<IKFAsyncResult*>(result);
    return KF_OK;
}