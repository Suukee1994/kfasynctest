#include <base/kf_base.hxx>
#include <base/kf_fast_list.hxx>

class FastList : public IKFFastList
{
    KF_IMPL_DECL_REFCOUNT;

    struct Entry
    {
        IKFBaseObject* Object;
        struct Entry *Next, *Prev;
    };
    Entry _anchor;
    int _count;

public:
    FastList() throw() : _ref_count(1), _count(0)
    { _anchor.Next = _anchor.Prev = &_anchor; _anchor.Object = nullptr; }
    virtual ~FastList() throw() { Clear(); }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_OBJECT_FAST_LIST)) {
            *ppv = static_cast<IKFFastList*>(this);
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
    virtual bool PushBack(IKFBaseObject* object)
    {
        auto entry = HoldObject(object);
        if (entry == nullptr)
            return false;

        AddToBack(entry);
        ++_count;
        return true;
    }

    virtual bool PushFront(IKFBaseObject* object)
    {
        auto entry = HoldObject(object);
        if (entry == nullptr)
            return false;

        AddToFront(entry);
        ++_count;
        return true;
    }

    virtual void PopBack(IKFBaseObject** object)
    {
        auto node = _anchor.Prev;
        Remove(node, object);
    }

    virtual void PopFront(IKFBaseObject** object)
    {
        auto node = _anchor.Next;
        Remove(node, object);
    }

    virtual bool GetBack(IKFBaseObject** object)
    {
        if (object == nullptr || _count == 0)
            return false;
        if (_anchor.Prev->Object == nullptr)
            return false;

        *object = _anchor.Prev->Object;
        (*object)->Retain();
        return true;
    }

    virtual bool GetFront(IKFBaseObject** object)
    {
        if (object == nullptr || _count == 0)
            return false;
        if (_anchor.Next->Object == nullptr)
            return false;

        *object = _anchor.Next->Object;
        (*object)->Retain();
        return true;
    }

    virtual void Clear()
    {
        auto node = _anchor.Next;
        while (node != &_anchor) {
            auto next = node->Next;
            node->Object->Recycle();
            free(node);
            node = next;
        }
        _anchor.Next = _anchor.Prev = &_anchor;
        _count = 0;
    }

    virtual bool IsEmpty() { return _anchor.Next == &_anchor; }
    virtual int GetCount() { return _count; }

    virtual void* Begin() { return IsEmpty() ? nullptr : _anchor.Next; }
    virtual void* End() { return nullptr; }
    virtual void* Next(void* node)
    {
        if (((Entry*)node)->Object == nullptr)
            return nullptr;
        return ((Entry*)node)->Next != &_anchor ? ((Entry*)node)->Next : nullptr;
    }
    virtual void* Prev(void* node)
    {
        if (node == nullptr)
            return _anchor.Prev;
        if (((Entry*)node)->Prev == &_anchor)
            return node;
        return ((Entry*)node)->Prev;
    }

    virtual void Get(void* node, IKFBaseObject** object)
    {
        auto entry = (Entry*)node;
        *object = entry->Object;
        (*object)->Retain();
    }
    virtual void Remove(void* node, IKFBaseObject** object)
    {
        auto entry = (Entry*)node;
        if (object)
            *object = entry->Object;
        else
            entry->Object->Recycle();

        NodeRemove(entry);
        free(entry);

        --_count;
    }

    virtual bool InsertFront(void* node, IKFBaseObject* object)
    {
        auto entry = HoldObject(object);
        if (entry == nullptr)
            return false;

        if (node == nullptr || ((Entry*)node)->Object == nullptr)
            AddToFront(entry);
        else
            NodeInsert(entry, ((Entry*)node)->Prev, (Entry*)node);
        ++_count;
        return true;
    }
    virtual bool InsertBack(void* node, IKFBaseObject* object)
    {
        auto entry = HoldObject(object);
        if (entry == nullptr)
            return false;

        if (node == nullptr || ((Entry*)node)->Object == nullptr)
            AddToBack(entry);
        else
            NodeInsert(entry, (Entry*)node, ((Entry*)node)->Next);
        ++_count;
        return true;
    }

private:
    Entry* HoldObject(IKFBaseObject* object)
    {
        if (object == nullptr)
            return nullptr;

        auto entry = (Entry*)malloc(sizeof(Entry));
        if (entry == nullptr)
            return nullptr;

        object->Retain();
        entry->Object = object;

        return entry;
    }

    inline void NodeInsert(Entry* node, Entry* prev, Entry* next)
    {
        node->Next = next;
        node->Prev = prev;
        prev->Next = node;
        next->Prev = node;
    }
    inline void NodeRemove(Entry* node)
    {
        node->Next->Prev = node->Prev;
        node->Prev->Next = node->Next;
    }
    
    //_anchor.Next = Front.
    inline void AddToFront(Entry* node) { NodeInsert(node, &_anchor, _anchor.Next); }
    //_anchor.Prev = Back.
    inline void AddToBack(Entry* node) { NodeInsert(node, _anchor.Prev, &_anchor); }
};

// ***************

KF_RESULT KFAPI KFCreateObjectFastList(IKFFastList** ppList)
{
    if (ppList == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) FastList();
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    *ppList = result;
    return KF_OK;
}

KF_RESULT KFAPI KFGetFastListObjectFromIndex(IKFFastList* list, KF_UINT32 index, IKFBaseObject** object)
{
    KF_UINT32 c = 0;
    auto it = list->Begin();
    auto end = list->End();
    while (it != end) {
        if (c == index) {
            list->Get(it, object);
            return KF_OK;
        }
        it = list->Next(it);
        ++c;
    }
    return KF_NOT_FOUND;
}

KF_RESULT KFAPI KFGetFastListNodeFromIndex(IKFFastList* list, KF_UINT32 index, void** ppNode)
{
    KF_UINT32 c = 0;
    auto it = list->Begin();
    auto end = list->End();
    while (it != end) {
        if (c == index) {
            *ppNode = it;
            return KF_OK;
        }
        it = list->Next(it);
        ++c;
    }
    return KF_NOT_FOUND;
}