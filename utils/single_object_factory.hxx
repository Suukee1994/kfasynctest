#ifndef __KF_UTIL__SINGLE_OBJECT_FACTORY_H
#define __KF_UTIL__SINGLE_OBJECT_FACTORY_H

#include <base/kf_base.hxx>

class SingleObjectFactory : public IKFObjectFactory
{
    KF_IMPL_DECL_REFCOUNT;
public:
    typedef bool (*CreateBaseObject)(IKFBaseObject**);
    typedef bool (*CreateBaseObjectWithConfig)(IKFBaseObject**, IKFBaseObject*);

    SingleObjectFactory() = delete;
    SingleObjectFactory(CreateBaseObject func, KIID iid) throw() : _ref_count(1)
    { CreateWorld = func; InterfaceId = iid; CreateWorldEx = nullptr; ConfigObject = nullptr; }
    SingleObjectFactory(CreateBaseObjectWithConfig func, KIID iid, IKFBaseObject* config)
    { CreateWorld = nullptr; InterfaceId = iid; CreateWorldEx = func; ConfigObject = config; config->Retain(); }
    virtual ~SingleObjectFactory() throw() { if (ConfigObject) ConfigObject->Recycle(); }

    KF_DISALLOW_COPY_AND_ASSIGN(SingleObjectFactory)

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_OBJECT_FACTORY)) {
            *ppv = static_cast<IKFObjectFactory*>(this);
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
    virtual KF_RESULT CreateObject(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (!_KFInterfaceIdEqual(interface_id, InterfaceId))
            return KF_NO_INTERFACE;
        
        IKFBaseObject* o = nullptr;
        if (CreateWorld) {
            if (!CreateWorld(&o))
                return KF_OUT_OF_MEMORY;
        }else if (CreateWorldEx) {
            if (!CreateWorldEx(&o, ConfigObject))
                return KF_OUT_OF_MEMORY;
        }

        auto r = o->CastToInterface(interface_id, ppv);
        o->Recycle();
        return r;
    }

private:
    CreateBaseObject CreateWorld;
    CreateBaseObjectWithConfig CreateWorldEx;
    KIID InterfaceId;
    IKFBaseObject* ConfigObject;
};

#endif //__KF_UTIL__SINGLE_OBJECT_FACTORY_H