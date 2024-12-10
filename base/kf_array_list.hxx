#ifndef __KF_BASE__ARRAY_LIST_H
#define __KF_BASE__ARRAY_LIST_H

#include <base/kf_base.hxx>

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_OBJECT_ARRAY_LIST "kf_iid_object_array_list"
#else
#define _KF_INTERFACE_ID_OBJECT_ARRAY_LIST "D30F93364CCB4CBFBD8DC8F7A86E3544"
#endif
struct IKFArrayList : public IKFBaseObject
{
    virtual bool AddElement(IKFBaseObject* obj) = 0;
    virtual bool GetElement(int index, IKFBaseObject** obj) = 0;
    virtual bool GetElementNoRef(int index, IKFBaseObject** obj) = 0;
    virtual bool InsertElementAt(int index, IKFBaseObject* obj) = 0;
    virtual bool RemoveAllElements() = 0;
    virtual bool RemoveElement(int index, IKFBaseObject** obj) = 0;
    virtual int GetElementCount() = 0;
};

KF_RESULT KFAPI KFCreateObjectArrayList(IKFArrayList** ppList);
KF_RESULT KFAPI KFCreateObjectArrayListFromInitCount(IKFArrayList** ppList, int initCount);

template<typename QType>
inline bool KFGetArrayListElement(IKFArrayList* list, int index, KIID iid, QType** object)
{
    *object = nullptr;
    IKFBaseObject* unk = nullptr;
    bool result = false;
    if (list->GetElement(index, &unk)) {
        result = (KFBaseGetInterface(unk, iid, object) == KF_OK);
        unk->Recycle();
    }
    return result;
}

#endif //__KF_BASE__ARRAY_LIST_H