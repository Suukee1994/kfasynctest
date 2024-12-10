#ifndef __KF_BASE__FAST_LIST_H
#define __KF_BASE__FAST_LIST_H

#include <base/kf_base.hxx>

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_OBJECT_FAST_LIST "kf_iid_object_fast_list"
#else
#define _KF_INTERFACE_ID_OBJECT_FAST_LIST "8C3B4EB91B474C06A0828515A0C1A8A0"
#endif
struct IKFFastList : public IKFBaseObject
{
    virtual bool PushBack(IKFBaseObject* object) = 0;
    virtual bool PushFront(IKFBaseObject* object) = 0;
    virtual void PopBack(IKFBaseObject** object) = 0;
    virtual void PopFront(IKFBaseObject** object) = 0;

    virtual bool GetBack(IKFBaseObject** object) = 0;
    virtual bool GetFront(IKFBaseObject** object) = 0;

    virtual void Clear() = 0;
    virtual bool IsEmpty() = 0;
    virtual int GetCount() = 0;

    virtual void* Begin() = 0;
    virtual void* End() = 0;
    virtual void* Next(void* node) = 0;
    virtual void* Prev(void* node) = 0;

    virtual void Get(void* node, IKFBaseObject** object) = 0;
    virtual void Remove(void* node, IKFBaseObject** object) = 0;

    virtual bool InsertFront(void* node, IKFBaseObject* object) = 0;
    virtual bool InsertBack(void* node, IKFBaseObject* object) = 0;
};

KF_RESULT KFAPI KFCreateObjectFastList(IKFFastList** ppList);
KF_RESULT KFAPI KFGetFastListObjectFromIndex(IKFFastList* list, KF_UINT32 index, IKFBaseObject** object);
KF_RESULT KFAPI KFGetFastListNodeFromIndex(IKFFastList* list, KF_UINT32 index, void** ppNode);

template<typename QType>
inline void KFGetFastListObject(IKFFastList* list, void* node, KIID iid, QType** object)
{
    *object = nullptr;
    IKFBaseObject* unk = nullptr;
    list->Get(node, &unk);
    unk->CastToInterface(iid, (void**)object);
    unk->Recycle();
}

#endif //__KF_BASE__FAST_LIST_H