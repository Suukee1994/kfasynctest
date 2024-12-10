#include <base/kf_base.hxx>
#include <base/kf_queue.hxx>

class ObjectQueue : public IKFQueue
{
    KF_IMPL_DECL_REFCOUNT;

    struct SingleEntry
    {
        IKFBaseObject* Object;
        struct SingleEntry* Next;
    };
    SingleEntry* _head;
    int _count;

public:
    ObjectQueue() throw() : _ref_count(1), _head(nullptr), _count(0) {}
    virtual ~ObjectQueue() throw() { Clear(); }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_OBJECT_QUEUE)) {
            *ppv = static_cast<IKFQueue*>(this);
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
    bool Enqueue(IKFBaseObject* object)
    {
        auto entry = (SingleEntry*)malloc(sizeof(SingleEntry));
        if (entry == nullptr)
            return false;

        entry->Object = object;
        entry->Next = nullptr;
        if (object)
            object->Retain();

        if (_head == nullptr) {
            _head = entry;
        }else{
            auto node = _head;
            while (node->Next != nullptr)
                node = node->Next;
            node->Next = entry;
        }

        ++_count;
        return true;
    }

    bool Dequeue(IKFBaseObject** object)
    {
        if (_head == nullptr)
            return false;

        auto cur = _head;
        if (object) {
            *object = cur->Object;
        }else{
            if (cur->Object)
                cur->Object->Recycle();
        }
        _head = cur->Next;
        free(cur);

        --_count;
        return true;
    }

    bool Requeue(IKFBaseObject* object)
    {
        if (_head == nullptr)
            return Enqueue(object);

        auto entry = (SingleEntry*)malloc(sizeof(SingleEntry));
        if (entry == nullptr)
            return false;

        entry->Object = object;
        entry->Next = _head;
        if (object)
            object->Retain();

        _head = entry;

        ++_count;
        return true;
    }

    void Clear()
    {
        while (_head != nullptr) {
            if (_head->Object)
                _head->Object->Recycle();
            auto next = _head->Next;
            free(_head);
            _head = next;
        }
        _count = 0;
    }

    IKFBaseObject* Peek()
    { return _head ? _head->Object : nullptr; }

    bool IsEmpty() { return _head == nullptr; }
    int GetCount() { return _count; }
};

// ***************

KF_RESULT KFAPI KFCreateObjectQueue(IKFQueue** ppQueue)
{
    if (ppQueue == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) ObjectQueue();
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    *ppQueue = result;
    return KF_OK;
}