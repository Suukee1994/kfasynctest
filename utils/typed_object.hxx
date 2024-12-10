#ifndef __KF_UTIL__TYPED_OBJECT_H
#define __KF_UTIL__TYPED_OBJECT_H

#include <base/kf_ptr.hxx>
#include <base/kf_attr.hxx>
#include <utils/auto_rwlock.hxx>

class TypedObject : public IKFBaseObject
{
    KF_IMPL_DECL_REFCOUNT;
public:
    static TypedObject* CreateTypedObject(int typeId) throw()
    { return new(std::nothrow) TypedObject(typeId); }

private:
    TypedObject() = delete;
    explicit TypedObject(int typeId) throw() : _ref_count(1) { _typeId = typeId; }
    virtual ~TypedObject() throw() {}

    KF_DISALLOW_COPY_AND_ASSIGN(TypedObject)

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT)) {
            *ppv = static_cast<IKFBaseObject*>(this);
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
    int GetTypeId() throw()
    {
        KFRWLock::AutoReadLock rlock(_rwlock);
        return _typeId;
    }
    void SetTypeId(int typeId) throw()
    {
        KFRWLock::AutoWriteLock wlock(_rwlock);
        _typeId = typeId;
    }

    bool GetAttributes(IKFAttributes** pAttrs) throw()
    {
        if (pAttrs == nullptr)
            return false;

        bool none = false;
        {
            KFRWLock::AutoReadLock rlock(_rwlock);
            if (_attrs == nullptr)
                none = true;
        }
        if (none) {
            KFRWLock::AutoWriteLock wlock(_rwlock);
            if (_attrs == nullptr)
                if (KF_FAILED(KFCreateAttributesWithoutObserver(&_attrs)))
                    return false;
        }

        KFRWLock::AutoReadLock rlock(_rwlock);
        if (_attrs) {
            *pAttrs = _attrs.Get();
            (*pAttrs)->Retain();
        }
        return true;
    }

private:
    KFRWLock _rwlock;

    int _typeId;
    KFPtr<IKFAttributes> _attrs;
};

#endif //__KF_UTIL__TYPED_OBJECT_H