#include <base/kf_base.hxx>
#include <base/kf_array_list.hxx>

#define _DEFAULT_LIST_COUNT 64

class ObjectList : public IKFArrayList
{
    KF_IMPL_DECL_REFCOUNT;

    struct MemoryPtr
    {
        unsigned char* Ptr;
        unsigned Size;

        MemoryPtr() throw() : Ptr(nullptr), Size(0) {}
        ~MemoryPtr() throw() { Free(); }

        bool Alloc(unsigned count, unsigned size) throw()
        {
            unsigned sz = count * size;
            if (Ptr == nullptr) {
                Ptr = (decltype(Ptr))malloc(sz + size);
                if (Ptr == nullptr)
                    return false;
                memset(Ptr, 0, sz);
                Size = sz;
            }else{
                if (sz > Size) {
                    if (Ptr)
                        Ptr = (decltype(Ptr))realloc(Ptr, sz * 2 + size);
                    if (Ptr == nullptr)
                        return false;
                    Size = sz * 2;
                }
            }
            return Ptr != nullptr;
        }
        void Free() throw()
        {
            if (Ptr)
                free(Ptr);
            Ptr = nullptr;
            Size = 0;
        }

        template<typename TResult = void*>
        TResult GetPtr() throw() { return reinterpret_cast<TResult>(Ptr); }
    };
    MemoryPtr _back_buffer0, _back_buffer1; //backBuffer
    MemoryPtr* _front_buffer;
    IKFBaseObject** _list; //frontBuffer
    int _count;

public:
    ObjectList() throw() : _ref_count(1), _count(0), _list(nullptr), _front_buffer(&_back_buffer0) {}
    virtual ~ObjectList() throw() { RemoveAllElements(); _back_buffer0.Free(); _back_buffer1.Free(); }

    bool Prepare(unsigned init_count) throw()
    {
        if (!_back_buffer0.Alloc(init_count, sizeof(IKFBaseObject*)) || !_back_buffer1.Alloc(init_count, sizeof(IKFBaseObject*)))
            return false;
        UpdateListPointerAddr();
        return true;
    }

private:
    inline void UpdateListPointerAddr() throw()
    { _list = _front_buffer->GetPtr<decltype(_list)>(); }
    inline bool IsFrontUseBackBuffer0() throw()
    { return _list == _back_buffer0.GetPtr<decltype(_list)>(); }
    inline void SwapBackBuffer() throw()
    {
        IsFrontUseBackBuffer0() ? _front_buffer = &_back_buffer1 : _front_buffer = &_back_buffer0;
        _list = _front_buffer->GetPtr<decltype(_list)>(); 
    }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_OBJECT_ARRAY_LIST)) {
            *ppv = static_cast<IKFArrayList*>(this);
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
    virtual bool AddElement(IKFBaseObject* obj)
    {
        if (obj == nullptr)
            return false;
        if (!_front_buffer->Alloc(_count + 1, sizeof(IKFBaseObject*)))
            return false;
        UpdateListPointerAddr();
        obj->Retain();
        _list[_count] = obj;
        ++_count;
        return true;
    }

    virtual bool GetElement(int index, IKFBaseObject** obj)
    {
        if (obj == nullptr)
            return false;
        if (index >= _count)
            return false;
        *obj = _list[index];
        (*obj)->Retain();
        return true;
    }

    virtual bool GetElementNoRef(int index, IKFBaseObject** obj)
    {
        if (obj == nullptr)
            return false;
        if (index >= _count)
            return false;
        *obj = _list[index];
        return true;
    }

    virtual bool InsertElementAt(int index, IKFBaseObject* obj)
    {
        if (obj == nullptr)
            return false;
        if (index > _count)
            return false;

        MemoryPtr* cur_ptr = _front_buffer;
        MemoryPtr* new_ptr = IsFrontUseBackBuffer0() ? &_back_buffer1 : &_back_buffer0;
        if (index == _count)
            return AddElement(obj);

        if (!new_ptr->Alloc(_count + 1, sizeof(IKFBaseObject*)))
            return false;
        if (index == 0) {
            memcpy(new_ptr->GetPtr<IKFBaseObject**>() + 1, cur_ptr->Ptr, _count * sizeof(IKFBaseObject*));
            *new_ptr->GetPtr<IKFBaseObject**>() = obj;
            obj->Retain();
        }else{
            memcpy(new_ptr->Ptr, cur_ptr->Ptr, (index + 1) * sizeof(IKFBaseObject*));
            new_ptr->GetPtr<IKFBaseObject**>()[index + 1] = obj;
            obj->Retain();
            memcpy(new_ptr->GetPtr<IKFBaseObject**>() + index + 2, cur_ptr->GetPtr<IKFBaseObject**>() + index + 1, (_count - index) * sizeof(IKFBaseObject*));
        }

        SwapBackBuffer();
        ++_count;
        return false;
    }

    virtual bool RemoveAllElements()
    {
        for (int i = 0; i < _count; i++)
            _list[i]->Recycle();

        _count = 0;
        _front_buffer = &_back_buffer0;
        _list = _back_buffer0.GetPtr<decltype(_list)>();
        return _list != nullptr;
    }

    virtual bool RemoveElement(int index, IKFBaseObject** obj)
    {
        if (index >= _count)
            return false;

        MemoryPtr* cur_ptr = _front_buffer;
        MemoryPtr* new_ptr = IsFrontUseBackBuffer0() ? &_back_buffer1 : &_back_buffer0;
        if (obj)
            *obj = _list[index];
        else
            _list[index]->Recycle();

        if (index + 1 == _count) {
            --_count;
            return true;
        }

        if (!new_ptr->Alloc(_count, sizeof(IKFBaseObject*)))
            return false;
        if (index == 0) {
            memcpy(new_ptr->Ptr, cur_ptr->GetPtr<IKFBaseObject**>() + 1, (_count - 1) * sizeof(IKFBaseObject*));
        }else{
            memcpy(new_ptr->Ptr, cur_ptr->Ptr, index * sizeof(IKFBaseObject*));
            memcpy(new_ptr->Ptr + index * sizeof(IKFBaseObject*), cur_ptr->Ptr + ((index + 1) * sizeof(IKFBaseObject*)), (_count - index) * sizeof(IKFBaseObject*));
        }

        SwapBackBuffer();
        --_count;
        return true;
    }

    virtual int GetElementCount() { return _count; }
};

// ***************

KF_RESULT KFAPI KFCreateObjectArrayList(IKFArrayList** ppList)
{
    if (ppList == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) ObjectList();
    if (result == nullptr || !result->Prepare(_DEFAULT_LIST_COUNT)) {
        if (result)
            result->Recycle();
        return result ? KF_UNEXPECTED : KF_OUT_OF_MEMORY;
    }

    *ppList = result;
    return KF_OK;
}

KF_RESULT KFAPI KFCreateObjectArrayListFromInitCount(IKFArrayList** ppList, int initCount)
{
    if (ppList == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) ObjectList();
    if (result == nullptr || !result->Prepare(initCount > 1 ? initCount : _DEFAULT_LIST_COUNT)) {
        if (result)
            result->Recycle();
        return result ? KF_UNEXPECTED : KF_OUT_OF_MEMORY;
    }

    *ppList = result;
    return KF_OK;
}