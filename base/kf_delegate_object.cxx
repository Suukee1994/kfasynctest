#include <base/kf_base.hxx>
#include <base/kf_delegate_object.hxx>

class DelegateObject : public IKFDelegateObject
{
    KF_IMPL_DECL_REFCOUNT;
public:
    DelegateObject() throw() : _ref_count(1)
    { _object = nullptr; RefObject = UnrefObject = nullptr; }
    virtual ~DelegateObject() throw()
    { if (_object) if (UnrefObject) UnrefObject(_object); }
    
public: //IKFBaseObject
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_DELEGATE_OBJECT)) {
            *ppv = static_cast<IKFDelegateObject*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }
    
    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }
    
public: //IKFDelegateObject
    virtual bool SetLifecycleCallback(KFDelegateObjectCallback refcb, KFDelegateObjectCallback unrefcb)
    {
        if (_object != nullptr)
            return false;
        
        RefObject = refcb;
        UnrefObject = unrefcb;
        return true;
    }
    
    virtual bool HasDelegateObject() { return _object != nullptr; }
    virtual bool GetDelegateObject(void** object)
    {
        if (_object == nullptr)
            return false;
        
        *object = _object;
        if (RefObject)
            RefObject(_object);
        return true;
    }
    virtual void SetDelegateObject(void* object)
    {
        if (_object)
            if (UnrefObject)
                UnrefObject(_object);
        
        _object = object;
        if (object)
            if (RefObject)
                RefObject(object);
    }
    
    virtual void* GetDelegateObjectNoRef() { return _object; }
    
private:
    void* _object;
    KFDelegateObjectCallback RefObject, UnrefObject;
};

// ***************

KF_RESULT KFAPI KFCreateDelegateObject(IKFDelegateObject** delegateObject)
{
    if (delegateObject == nullptr)
        return KF_INVALID_PTR;
    
    auto result = new(std::nothrow) DelegateObject();
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;
    
    *delegateObject = result;
    return KF_OK;
}

// ***************

void KFAPI GlobalKFBaseObjectDelegateRefCallback(void* object)
{
    IKFBaseObject* o = (IKFBaseObject*)object;
    o->Retain();
}
void KFAPI GlobalKFBaseObjectDelegateUnrefCallback(void* object)
{
    IKFBaseObject* o = (IKFBaseObject*)object;
    o->Recycle();
}

KF_RESULT KFAPI KFCreateDelegateObjectWrapper(IKFDelegateObject** delegateObject, IKFBaseObject* object)
{
    if (delegateObject == nullptr)
        return KF_INVALID_PTR;
    if (object == nullptr)
        return KF_INVALID_ARG;

    IKFDelegateObject* temp = nullptr;
    auto result = KFCreateDelegateObject(&temp);
    _KF_FAILED_RET(result);

    temp->SetLifecycleCallback(&GlobalKFBaseObjectDelegateRefCallback, &GlobalKFBaseObjectDelegateUnrefCallback);
    temp->SetDelegateObject(object);

    *delegateObject = temp;
    return KF_OK;
}