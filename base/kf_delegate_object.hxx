#ifndef __KF_BASE__DELEGATE_H
#define __KF_BASE__DELEGATE_H

#include <base/kf_base.hxx>

typedef void (KFAPI* KFDelegateObjectCallback)(void*);

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_DELEGATE_OBJECT "kf_iid_delegate_object"
#else
#define _KF_INTERFACE_ID_DELEGATE_OBJECT "6C66056983134D3DAB537D8266070339"
#endif
#define _KF_INTERFACE_ID_BOUNDARY_OBJECT _KF_INTERFACE_ID_DELEGATE_OBJECT
struct IKFDelegateObject : public IKFBaseObject
{
    virtual bool SetLifecycleCallback(KFDelegateObjectCallback refcb, KFDelegateObjectCallback unrefcb) = 0;
    
    virtual bool HasDelegateObject() = 0;
    virtual bool GetDelegateObject(void** object) = 0;
    virtual void SetDelegateObject(void* object) = 0;
    
    virtual void* GetDelegateObjectNoRef() = 0;
};
typedef IKFDelegateObject IKFBoundaryObject;

KF_RESULT KFAPI KFCreateDelegateObject(IKFDelegateObject** delegateObject);
KF_RESULT KFAPI KFCreateDelegateObjectWrapper(IKFDelegateObject** delegateObject, IKFBaseObject* object);

#endif //__KF_BASE__DELEGATE_H