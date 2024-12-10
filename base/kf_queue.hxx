#ifndef __KF_BASE__QUEUE_H
#define __KF_BASE__QUEUE_H

#include <base/kf_base.hxx>

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_OBJECT_QUEUE "kf_iid_object_queue"
#else
#define _KF_INTERFACE_ID_OBJECT_QUEUE "CE79C3C2775A43BABCF0F4D4410D962B"
#endif
struct IKFQueue : public IKFBaseObject
{
    virtual bool Enqueue(IKFBaseObject* object) = 0;
    virtual bool Dequeue(IKFBaseObject** object) = 0;
    virtual bool Requeue(IKFBaseObject* object) = 0;
    virtual void Clear() = 0;
    virtual IKFBaseObject* Peek() = 0;
    virtual bool IsEmpty() = 0;
    virtual int GetCount() = 0;
};

KF_RESULT KFAPI KFCreateObjectQueue(IKFQueue** ppQueue);

#endif //__KF_BASE__QUEUE_H