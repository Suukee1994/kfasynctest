#include <utils/auto_rwlock.hxx>
#include <base/kf_base.hxx>
#include <base/kf_attr.hxx>
#include <base/kf_array_list.hxx>
#include <async/kf_async_abstract.hxx>

class Attributes : public IKFAttributes, private IKFAsyncCallback
{
    KF_IMPL_DECL_REFCOUNT;

    IKFArrayList* _list;
    IKFArrayList* _observers;

    KF_ATTRIBUTE_OBSERVER_THREAD_MODE _observer_mode;
    KASYNCOBJECT _observer_worker;

    KFRWLock _rwlock;

    struct SavedMemoryHead
    {
        KF_ATTRIBUTE_TYPE type;
        KF_UINT32 nameSize, dataSize;
    };
    struct SavedMemoryData
    {
        SavedMemoryHead head;
        unsigned char data;
    };

private:
    class Store : public IKFBaseObject
    {
    public:
        struct Types
        {
            KF_ATTRIBUTE_TYPE type;
            union
            {
                KF_UINT32 val32bit;
                KF_UINT64 val64bit;
                double valFloat;
                char* string;
                IKFBuffer* binary;
                IKFBaseObject* object;
            };
            KF_UINT32 dataLengthInBytes;
        };

    private:
        char* _name;
        Types _type;

        int _state;
        char* _reference;

        KREF _ref_count;

    public:
        Store() throw() : _ref_count(1), _state(-1), _reference(nullptr)
        { _name = nullptr; _type.type = KF_ATTRIBUTE_TYPE::KF_ATTR_INVALID; }
        virtual ~Store() throw() { Clear(); if (_name) free(_name); if (_reference) free(_reference); }

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
        void SetName(const char* name) throw()
        {
            if (_name)
                free(_name);
            _name = strdup(name);
        }
        const char* GetName() const throw() { return _name; }

        void SetReference(const char* reference) throw()
        {
            if (_reference)
                free(_reference);
            _reference = strdup(reference);
        }
        const char* GetReference() const throw() { return _reference; }

        void SetState(int state) throw() { _state = state; }
        int GetState() throw() { return _state; }

        void SetUINT32(KF_UINT32 value) throw()
        {
            Clear();
            _type.type = KF_ATTRIBUTE_TYPE::KF_ATTR_UINT32;
            _type.val32bit = value;
            _type.dataLengthInBytes = sizeof(KF_UINT32);
        }
        void SetUINT64(KF_UINT64 value) throw()
        {
            Clear();
            _type.type = KF_ATTRIBUTE_TYPE::KF_ATTR_UINT64;
            _type.val64bit = value;
            _type.dataLengthInBytes = sizeof(KF_UINT64);
        }
        void SetDouble(double value) throw()
        {
            Clear();
            _type.type = KF_ATTRIBUTE_TYPE::KF_ATTR_DOUBLE;
            _type.valFloat = value;
            _type.dataLengthInBytes = sizeof(double);
        }

        bool SetString(const char* str) throw()
        {
            Clear();
            if (str == nullptr)
                return false;

            size_t len = strlen(str);
            if (len == 0)
                return false;

            ++len;
            _type.type = KF_ATTRIBUTE_TYPE::KF_ATTR_STRING;
            _type.dataLengthInBytes = (KF_UINT32)len;
            _type.string = (char*)malloc(KF_ALLOC_ALIGNED(len));
            if (_type.string == nullptr)
                return false;

            strcpy(_type.string, str);
            return true;
        }
        bool SetBinary(const void* ptr, int size) throw()
        {
            Clear();
            if (ptr == nullptr || size == 0)
                return false;

            _type.type = KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY;
            _type.dataLengthInBytes = size;
            if (KF_FAILED(KFCreateMemoryBuffer(size, &_type.binary)))
                return false;

            memcpy(_type.binary->GetAddress(), ptr, size);
            _type.binary->SetCurrentLength(size);
            return true;
        }

        bool SetObject(IKFBaseObject* obj) throw()
        {
            Clear();
            if (obj == nullptr)
                return false;
            if (obj == this)
                return false;

            _type.type = KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT;
            _type.object = obj;
            _type.object->Retain();
            return true;
        }

        Types* GetTypes() throw() { return &_type; }
        void Clear() throw()
        {
            if (_type.type == KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT)
                _type.object->Recycle();
            else if (_type.type == KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY)
                _type.binary->Recycle();
            else if (_type.type == KF_ATTRIBUTE_TYPE::KF_ATTR_STRING)
                free(_type.string);

            _type.type = KF_ATTRIBUTE_TYPE::KF_ATTR_INVALID;
        }
    };

public:
    Attributes() throw() : _ref_count(1), _list(nullptr),
    _observer_mode(KF_ATTR_OBSERVER_THREAD_DIRECT_CALL), _observer_worker(nullptr) {}
    virtual ~Attributes() throw() { Uninitialize(); }

    bool Initialize(bool no_use_observer = false) throw()
    {
        if (KF_FAILED(KFCreateObjectArrayList(&_list)))
            return false;

        _observers = nullptr;
        if (!no_use_observer) {
            if (KF_FAILED(KFCreateObjectArrayList(&_observers)))
                return false;
        }
        return true;
    }

    void Uninitialize() throw()
    {
        if (_observers != nullptr) {
            int count = _observers->GetElementCount();
            for (int i = 0; i < count; i++) {
                IKFBaseObject* obj = nullptr;
                if (_observers->GetElementNoRef(i, &obj)) {
                    IKFAttributesObserver* observer = nullptr;
                    KFBaseGetInterface(obj, _KF_INTERFACE_ID_ATTRIBUTES_OBSERVER, &observer);
                    if (observer)
                        observer->OnDestroyAttributes();
                }
            }
        }

        KF_SAFE_RELEASE(_list);
        KF_SAFE_RELEASE(_observers);

        if (_observer_worker)
            KFAsyncDestroyWorker(_observer_worker);
    }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_ATTRIBUTES))
            *ppv = static_cast<IKFAttributes*>(this);
        else if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_ASYNC_CALLBACK))
            *ppv = static_cast<IKFAsyncCallback*>(this);
        else if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_OBJECT_READWRITE))
            *ppv = static_cast<IKFObjectReadWrite*>(this);
        else
            return KF_NO_INTERFACE;

        Retain();
        return KF_OK;
    }

    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }

public:
    virtual KF_ATTRIBUTE_TYPE GetItemType(const char* key)
    {
        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        return s ? s->GetTypes()->type : KF_ATTRIBUTE_TYPE::KF_ATTR_INVALID;
    }

    virtual KF_RESULT GetItemName(int index, IKFBuffer** name)
    {
        if (name == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        if (index >= _list->GetElementCount())
            return KF_INVALID_ARG;

        IKFBaseObject* obj = nullptr;
        if (!_list->GetElement(index, &obj))
            return KF_INVALID_DATA;

        auto s = static_cast<Store*>(obj);
        int len = (int)strlen(s->GetName());
        auto r = KFCreateMemoryBuffer(len + 1, name);
        if (KF_FAILED(r)) {
            obj->Recycle();
            return r;
        }

        memcpy((*name)->GetAddress(), s->GetName(), len);
        (*name)->SetCurrentLength(len + 1);
        obj->Recycle();
        return KF_OK;
    }

    virtual int GetItemCount()
    { KFRWLock::AutoReadLock rlock(_rwlock); return _list->GetElementCount(); }

    virtual KF_RESULT GetUINT32(const char* key, KF_UINT32* value)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (value == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_UINT32)
            return KF_INVALID_DATA;

        *value = s->GetTypes()->val32bit;
        return KF_OK;
    }

    virtual KF_RESULT GetUINT64(const char* key, KF_UINT64* value)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (value == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_UINT64)
            return KF_INVALID_DATA;

        *value = s->GetTypes()->val64bit;
        return KF_OK;
    }

    virtual KF_RESULT GetDouble(const char* key, double* value)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (value == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_DOUBLE)
            return KF_INVALID_DATA;

        *value = s->GetTypes()->valFloat;
        return KF_OK;
    }

    virtual KF_RESULT GetStringLength(const char* key, KF_UINT32* len)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (len == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_STRING)
            return KF_INVALID_DATA;

        *len = (KF_UINT32)strlen(s->GetTypes()->string);
        return KF_OK;
    }

    virtual KF_RESULT GetString(const char* key, char* string, KF_UINT32 str_ptr_len)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (string == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_STRING)
            return KF_INVALID_DATA;

        memset(string, 0, str_ptr_len);
        KF_UINT32 len = (KF_UINT32)strlen(s->GetTypes()->string) + 1;
        if (str_ptr_len > 0)
            memcpy(string, s->GetTypes()->string, len > str_ptr_len ? str_ptr_len : len);
        else
            strcpy(string, s->GetTypes()->string);
        return KF_OK;
    }

    virtual KF_RESULT GetStringAlloc(const char* key, IKFBuffer** buffer)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (buffer == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_STRING)
            return KF_INVALID_DATA;

        IKFBuffer* buf = nullptr;
        auto r = KFCreateMemoryBuffer((int)strlen(s->GetTypes()->string) + 1, &buf);
        _KF_FAILED_RET(r);

        strcpy(KFGetBufferAddress<char*>(buf), s->GetTypes()->string);
        buf->SetCurrentLength(buf->GetMaxLength());
        *buffer = buf;
        return KF_OK;
    }

    virtual KF_RESULT GetBlobLength(const char* key, KF_UINT32* len)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (len == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY)
            return KF_INVALID_DATA;

        *len = (KF_UINT32)s->GetTypes()->binary->GetCurrentLength();
        return KF_OK;
    }

    virtual KF_RESULT GetBlob(const char* key, void* buf, KF_UINT32 buf_ptr_len)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (buf == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY)
            return KF_INVALID_DATA;

        auto bin = s->GetTypes()->binary;
        auto len = bin->GetCurrentLength();
        if (buf_ptr_len > 0)
            memcpy(buf, bin->GetAddress(), len > (int)buf_ptr_len ? (int)buf_ptr_len : len);
        else
            memcpy(buf, bin->GetAddress(), bin->GetCurrentLength());
        return KF_OK;
    }

    virtual KF_RESULT GetBlobAlloc(const char* key, IKFBuffer** buffer)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (buffer == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY)
            return KF_INVALID_DATA;

        auto bin = s->GetTypes()->binary;
        IKFBuffer* buf = nullptr;
        auto r = KFCreateMemoryBuffer((int)bin->GetMaxLength(), &buf);
        _KF_FAILED_RET(r);

        *buffer = buf;
        return KFCopyMemoryBuffer(bin, buf);
    }

    virtual KF_RESULT SetUINT32(const char* key, KF_UINT32 value)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;

        KFRWLock::AutoWriteLock wlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr) {
            s = CreateNewElement(key);
            if (s == nullptr)
                return KF_OUT_OF_MEMORY;
        }

        s->SetUINT32(value);
        NotifyAllObserver(key, true);
        return KF_OK;
    }

    virtual KF_RESULT SetUINT64(const char* key, KF_UINT64 value)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;

        KFRWLock::AutoWriteLock wlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr) {
            s = CreateNewElement(key);
            if (s == nullptr)
                return KF_OUT_OF_MEMORY;
        }

        s->SetUINT64(value);
        NotifyAllObserver(key, true);
        return KF_OK;
    }

    virtual KF_RESULT SetDouble(const char* key, double value)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;

        KFRWLock::AutoWriteLock wlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr) {
            s = CreateNewElement(key);
            if (s == nullptr)
                return KF_OUT_OF_MEMORY;
        }

        s->SetDouble(value);
        NotifyAllObserver(key, true);
        return KF_OK;
    }

    virtual KF_RESULT SetString(const char* key, const char* value)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (value == nullptr)
            return KF_INVALID_DATA;

        KFRWLock::AutoWriteLock wlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr) {
            s = CreateNewElement(key);
            if (s == nullptr)
                return KF_OUT_OF_MEMORY;
        }

        if (!s->SetString(value))
            return KF_ERROR;

        NotifyAllObserver(key, true);
        return KF_OK;
    }

    virtual KF_RESULT SetBlob(const char* key, const void* buf, KF_UINT32 buf_size)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (buf == nullptr || buf_size == 0)
            return KF_INVALID_DATA;

        KFRWLock::AutoWriteLock wlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr) {
            s = CreateNewElement(key);
            if (s == nullptr)
                return KF_OUT_OF_MEMORY;
        }

        if (!s->SetBinary(buf, buf_size))
            return KF_ERROR;

        NotifyAllObserver(key, true);
        return KF_OK;
    }

    virtual KF_RESULT GetObject(const char* key, KIID iid, void** ppv)
    {
        if (key == nullptr || iid == nullptr)
            return KF_INVALID_ARG;
        if (ppv == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr)
            return KF_ERROR;

        if (s->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT)
            return KF_INVALID_DATA;

        return s->GetTypes()->object->CastToInterface(iid, ppv);
    }

    virtual KF_RESULT SetObject(const char* key, IKFBaseObject* object)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;
        if (object == nullptr)
            return KF_INVALID_OBJECT;
        if (object == static_cast<IKFAttributes*>(this))
            return KF_ABORT;

        KFRWLock::AutoWriteLock wlock(_rwlock);
        auto s = SearchStore(key);
        if (s == nullptr) {
            s = CreateNewElement(key);
            if (s == nullptr)
                return KF_OUT_OF_MEMORY;
        }

        return s->SetObject(object) ? KF_OK : KF_ERROR;
    }

    virtual KF_RESULT DeleteItem(const char* key)
    {
        if (key == nullptr)
            return KF_INVALID_ARG;

        KFRWLock::AutoWriteLock wlock(_rwlock);
        int index = 0;
        auto s = SearchStore(key, &index);
        if (s == nullptr)
            return KF_NOT_FOUND;

        _list->RemoveElement(index, nullptr);

        NotifyAllObserver(key, false);
        return KF_OK;
    }

    virtual KF_RESULT DeleteAllItems()
    {
        KFRWLock::AutoWriteLock wlock(_rwlock);
        _list->RemoveAllElements();
        return KF_OK;
    }

    virtual KF_RESULT HasItem(const char* key)
    {
        KFRWLock::AutoReadLock rlock(_rwlock);
        return SearchStore(key) ? KF_OK : KF_NOT_FOUND;
    }

    virtual KF_RESULT CopyItem(const char* key, IKFAttributes* copyTo)
    {
        if (copyTo == this)
            return KF_INVALID_INPUT;
        if (copyTo == nullptr)
            return KF_INVALID_ARG;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key, nullptr);
        if (s == nullptr)
            return KF_NOT_FOUND;

        if (KF_SUCCEEDED(copyTo->HasItem(key)))
            copyTo->DeleteItem(key);

        KF_RESULT r = KF_OK;
        switch (s->GetTypes()->type) {
        case KF_ATTRIBUTE_TYPE::KF_ATTR_INVALID:
            r = KF_INVALID_DATA;
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT32:
            r = copyTo->SetUINT32(s->GetName(), s->GetTypes()->val32bit);
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT64:
            r = copyTo->SetUINT64(s->GetName(), s->GetTypes()->val64bit);
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_DOUBLE:
            r = copyTo->SetDouble(s->GetName(), s->GetTypes()->valFloat);
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_STRING:
            r = copyTo->SetString(s->GetName(), s->GetTypes()->string);
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY:
            r = copyTo->SetBlob(s->GetName(),
                                s->GetTypes()->binary->GetAddress(),
                                s->GetTypes()->binary->GetCurrentLength());
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT:
            r = copyTo->SetObject(s->GetName(), s->GetTypes()->object);
            break;
        }
        return r;
    }

    virtual KF_RESULT CopyAllItems(IKFAttributes* copyTo)
    {
        if (copyTo == this)
            return KF_INVALID_INPUT;
        if (copyTo == nullptr)
            return KF_INVALID_ARG;

        KFRWLock::AutoReadLock rlock(_rwlock);
        int count = _list->GetElementCount();
        for (int i = 0; i < count; i++) {
            IKFBaseObject* obj = nullptr;
            if (_list->GetElementNoRef(i, &obj)) {
                KF_RESULT r = KF_OK;
                auto store = static_cast<Store*>(obj);
                switch (store->GetTypes()->type) {
                case KF_ATTRIBUTE_TYPE::KF_ATTR_INVALID:
                    r = KF_INVALID_DATA;
                    break;
                case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT32:
                    r = copyTo->SetUINT32(store->GetName(), store->GetTypes()->val32bit);
                    break;
                case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT64:
                    r = copyTo->SetUINT64(store->GetName(), store->GetTypes()->val64bit);
                    break;
                case KF_ATTRIBUTE_TYPE::KF_ATTR_DOUBLE:
                    r = copyTo->SetDouble(store->GetName(), store->GetTypes()->valFloat);
                    break;
                case KF_ATTRIBUTE_TYPE::KF_ATTR_STRING:
                    r = copyTo->SetString(store->GetName(), store->GetTypes()->string);
                    break;
                case KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY:
                    r = copyTo->SetBlob(store->GetName(),
                                        store->GetTypes()->binary->GetAddress(),
                                        store->GetTypes()->binary->GetCurrentLength());
                    break;
                case KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT:
                    r = copyTo->SetObject(store->GetName(), store->GetTypes()->object);
                    break;
                }

                _KF_FAILED_RET(r);
            }
        }
        return KF_OK;
    }

    virtual KF_RESULT MatchItem(const char* key, IKFAttributes* other_attr)
    {
        if (key == nullptr || other_attr == nullptr)
            return KF_INVALID_ARG;
        if (this == other_attr)
            return KF_OK;

        KFRWLock::AutoReadLock rlock(_rwlock);
        auto s = SearchStore(key, nullptr);
        if (s == nullptr)
            return KF_NOT_FOUND;

        if (s->GetTypes()->type != other_attr->GetItemType(key))
            return KF_NO_MATCH;

        IKFBuffer* buffer = nullptr;
        IKFBaseObject* object = nullptr;
        auto result = KF_OK;
        switch (s->GetTypes()->type) {
        case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT32:
            if (s->GetTypes()->val32bit != KFGetAttributeUINT32(other_attr, key, s->GetTypes()->val32bit + 1))
                result = KF_NO_MATCH;
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT64:
            if (s->GetTypes()->val64bit != KFGetAttributeUINT64(other_attr, key, s->GetTypes()->val64bit + 1))
                result = KF_NO_MATCH;
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_DOUBLE:
            if (s->GetTypes()->valFloat != KFGetAttributeDouble(other_attr, key, s->GetTypes()->valFloat + 0.1))
                result = KF_NO_MATCH;
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_STRING:
            other_attr->GetStringAlloc(key, &buffer);
            if (buffer) {
                if (strcmp(KFGetBufferAddress<const char*>(buffer), s->GetTypes()->string) != 0)
                    result = KF_NO_MATCH;
            }else{
                result = KF_NO_MATCH;
            }
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY:
            other_attr->GetBlobAlloc(key, &buffer);
            if (buffer) {
                if (buffer->GetCurrentLength() == s->GetTypes()->binary->GetCurrentLength()) {
                    if (memcmp(buffer->GetAddress(), s->GetTypes()->binary->GetAddress(), buffer->GetCurrentLength()) != 0)
                        result = KF_NO_MATCH;
                }else{
                    result = KF_NO_MATCH;
                }
            }else{
                result = KF_NO_MATCH;
            }
            break;
        case KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT:
            KFBaseGetInterface(other_attr, _KF_INTERFACE_ID_BASE_OBJECT, &object);
            if (object != s->GetTypes()->object)
                result = KF_NO_MATCH;
            break;
        default:
            result = KF_NO_MATCH;
        }

        if (buffer)
            buffer->Recycle();
        if (object)
            object->Recycle();
        return result;
    }

    KF_RESULT MatchAllItems(IKFAttributes* other_attr)
    {
        if (other_attr == nullptr)
            return KF_INVALID_INPUT;

        KFRWLock::AutoReadLock rlock(_rwlock);
        int count = _list->GetElementCount();
        for (int i = 0; i < count; i++) {
            IKFBaseObject* obj = nullptr;
            if (_list->GetElementNoRef(i, &obj)) {
                auto store = static_cast<Store*>(obj);
                auto result = other_attr->MatchItem(store->GetName(), this);
                _KF_FAILED_RET(result);
            }
        }
        return KF_OK;
    }

    virtual KF_RESULT SaveToBuffer(IKFBuffer** buffer)
    {
        //内存格式：4字节类型 + 4字节Name长度 + 4字节数据长度 + Name + NULL(0) + Data.
        if (buffer == nullptr)
            return KF_INVALID_PTR;

        KFRWLock::AutoReadLock rlock(_rwlock);
        int count = _list->GetElementCount();
        int alloc_size = 4;
        int processed_count = 0;
        for (int i = 0; i < count; i++) {
            IKFBaseObject* obj = nullptr;
            if (_list->GetElementNoRef(i, &obj)) {
                auto store = static_cast<Store*>(obj);
                if (store->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT) {
                    if (store->GetTypes()->type == KF_ATTRIBUTE_TYPE::KF_ATTR_INVALID)
                        return KF_INVALID_OBJECT;

                    alloc_size = alloc_size + sizeof(SavedMemoryHead) + store->GetTypes()->dataLengthInBytes;
                    alloc_size = alloc_size + (int)strlen(store->GetName()) + 1;
                    ++processed_count;
                }
            }
        }

        IKFBuffer* buf = nullptr;
        auto r = KFCreateMemoryBuffer(alloc_size, &buf);
        _KF_FAILED_RET(r);

        *(KF_UINT32*)buf->GetAddress() = processed_count;
        auto offset = buf->GetAddress() + sizeof(KF_UINT32);

        for (int i = 0; i < count; i++) {
            IKFBaseObject* obj = nullptr;
            if (_list->GetElementNoRef(i, &obj)) {
                auto store = static_cast<Store*>(obj);
                if (store->GetTypes()->type != KF_ATTRIBUTE_TYPE::KF_ATTR_OBJECT) {
                    SavedMemoryHead head;
                    head.type = store->GetTypes()->type;
                    head.nameSize = (KF_UINT32)strlen(store->GetName());
                    head.dataSize = store->GetTypes()->dataLengthInBytes;

                    *(SavedMemoryHead*)offset = head;
                    offset += sizeof(SavedMemoryHead);

                    memcpy(offset, store->GetName(), head.nameSize);
                    offset += head.nameSize;
                    *offset = 0; //NULL with string.
                    ++offset;

                    switch (store->GetTypes()->type) {
                    case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT32:
                        *(KF_UINT32*)offset = store->GetTypes()->val32bit;
                        break;
                    case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT64:
                        *(KF_UINT64*)offset = store->GetTypes()->val64bit;
                        break;
                    case KF_ATTRIBUTE_TYPE::KF_ATTR_DOUBLE:
                        *(double*)offset = store->GetTypes()->valFloat;
                        break;
                    case KF_ATTRIBUTE_TYPE::KF_ATTR_STRING:
                        memcpy(offset, store->GetTypes()->string, head.dataSize);
                        break;
                    case KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY:
                        memcpy(offset, store->GetTypes()->binary->GetAddress(), head.dataSize);
                        break;
                    default:
                        return KF_INVALID_DATA;
                    }
                    offset += head.dataSize;
                }
            }
        }

        buf->SetCurrentLength(alloc_size);
        *buffer = buf;
        return KF_OK;
    }

    virtual KF_RESULT LoadFromBuffer(IKFBuffer* buffer)
    {
        if (buffer == nullptr)
            return KF_INVALID_ARG;

        if (buffer->GetCurrentLength() < (sizeof(KF_UINT32) + sizeof(SavedMemoryHead) + 2))
            return KF_INVALID_DATA;

        auto offset = buffer->GetAddress();
        int count = (int)(*(KF_UINT32*)offset);
        offset += sizeof(KF_UINT32);

        KF_RESULT r = KF_OK;
        for (int i = 0; i < count; i++) {
            SavedMemoryData* data = (SavedMemoryData*)offset;
            offset += sizeof(SavedMemoryHead);
            char* name = (char*)offset;

            offset += (data->head.nameSize + 1);
            switch (data->head.type) {
            case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT32:
                r = SetUINT32(name, *(KF_UINT32*)offset);
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_UINT64:
                r = SetUINT64(name, *(KF_UINT64*)offset);
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_DOUBLE:
                r = SetDouble(name, *(double*)offset);
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_STRING:
                r = SetString(name, (char*)offset);
                break;
            case KF_ATTRIBUTE_TYPE::KF_ATTR_BINARY:
                r = SetBlob(name, offset, data->head.dataSize);
                break;
            default:
                return KF_INVALID_DATA;
            }
            offset += data->head.dataSize;
        }
        return r;
    }

    virtual KF_RESULT ChangeObserverThreadMode(KF_ATTRIBUTE_OBSERVER_THREAD_MODE mode)
    {
        if (_observers == nullptr)
            return KF_NOT_SUPPORTED;

        if (mode == KF_ATTR_OBSERVER_THREAD_ASYNC_POOL && _observer_worker == nullptr) {
            auto r = KFAsyncCreateWorker(false, 1, &_observer_worker);
            _KF_FAILED_RET(r);
        }

        _observer_mode = mode;
        return KF_OK;
    }

    virtual KF_RESULT AddObserver(IKFAttributesObserver* observer)
    {
        if (observer == nullptr || _observers == nullptr)
            return observer ? KF_INVALID_ARG : KF_NOT_SUPPORTED;

        int index = SearchObserverIndex(observer);
        if (index != -1)
            RemoveObserver(observer);

        return _observers->AddElement(observer) ? KF_OK : KF_OUT_OF_MEMORY;
    }

    virtual KF_RESULT RemoveObserver(IKFAttributesObserver* observer)
    {
        if (observer == nullptr || _observers == nullptr)
            return observer ? KF_INVALID_ARG : KF_NOT_SUPPORTED;

        int index = SearchObserverIndex(observer);
        if (index != -1)
            _observers->RemoveElement(index, nullptr);

        return KF_OK;
    }

public: //IKFObjectReadWrite
    virtual int GetStreamLength()
    {
        IKFBuffer* buf = nullptr;
        auto r = SaveToBuffer(&buf);
        _KF_FAILED_RET(r);

        int result = buf->GetCurrentLength();
        buf->Recycle();
        return result;
    }

    virtual KF_RESULT Read(KF_UINT8* buffer, int* length)
    {
        if (buffer == nullptr || length == nullptr)
            return KF_INVALID_PTR;

        IKFBuffer* buf = nullptr;
        auto r = SaveToBuffer(&buf);
        _KF_FAILED_RET(r);

        *length = buf->GetCurrentLength();
        memcpy(buffer, buf->GetAddress(), buf->GetCurrentLength());
        buf->Recycle();
        return KF_OK;
    }
    virtual KF_RESULT Write(KF_UINT8* buffer, int length)
    {
        if (buffer == nullptr || length == 0)
            return KF_INVALID_ARG;

        IKFBuffer* buf = nullptr;
        auto r = KFCreateMemoryBufferFixed(buffer, length, &buf);
        _KF_FAILED_RET(r);

        r = LoadFromBuffer(buf);
        _KF_FAILED_RET(r);

        buf->Recycle();
        return KF_OK;
    }

private:
    Store* SearchStore(const char* key, int* index = nullptr)
    {
        if (key == nullptr || strlen(key) == 0)
            return nullptr;

        int count = _list->GetElementCount();
        if (count == 0)
            return nullptr;

        for (int i = 0; i < count; i++) {
            IKFBaseObject* obj = nullptr;
            if (_list->GetElementNoRef(i, &obj)) {
                auto store = static_cast<Store*>(obj);
                if (strcmp(key, store->GetName()) == 0) {
                    if (index)
                        *index = i;
                    return store;
                }
            }
        }

        return nullptr;
    }

    Store* CreateNewElement(const char* key)
    {
        auto store = new(std::nothrow) Store();
        if (store == nullptr)
            return nullptr;

        _list->AddElement(store);
        store->SetName(key);
        store->Recycle();
        return store;
    }

    int SearchObserverIndex(IKFAttributesObserver* observer)
    {
        int index = -1;
        int count = _observers->GetElementCount();
        for (int i = 0; i < count; i++) {
            IKFBaseObject* obj = nullptr;
            if (_observers->GetElementNoRef(i, &obj)) {
                if (obj == observer) {
                    index = i;
                    break;
                }
            }
        }
        return index;
    }

    void InvokeAllObserver(const char* key, bool change_or_delete)
    {
        int count = _observers->GetElementCount();
        for (int i = 0; i < count; i++) {
            IKFBaseObject* obj = nullptr;
            if (_observers->GetElementNoRef(i, &obj)) {
                IKFAttributesObserver* observer = nullptr;
                KFBaseGetInterface(obj, _KF_INTERFACE_ID_ATTRIBUTES_OBSERVER, &observer);
                if (observer) {
                    if (change_or_delete)
                        observer->OnAttributeChanged(key);
                    else
                        observer->OnAttributeDeleted(key);
                    observer->Recycle();
                }
            }
        }
    }

    void NotifyAllObserver(const char* key, bool change_or_delete)
    {
        if (_observers == nullptr)
            return;

        if (_observer_mode == KF_ATTR_OBSERVER_THREAD_DIRECT_CALL)
            InvokeAllObserver(key, change_or_delete);
        else if (_observer_mode == KF_ATTR_OBSERVER_THREAD_ASYNC_POOL)
            NotifyObserverAsync(key, change_or_delete);
    }

    void NotifyObserverAsync(const char* key, bool change_or_delete)
    {
        auto store = new(std::nothrow) Store();
        if (store) {
            store->SetName(key);
            store->SetState(change_or_delete ? 0 : 1);

            KFAsyncPutWorkItem(_observer_worker, this, store);
            store->Recycle();
        }
    }

private:
    virtual void Execute(IKFAsyncResult* pResult) //from NotifyObserverAsync
    {
        IKFBaseObject* obj = nullptr;
        if (pResult && pResult->GetState(&obj)) {
            auto store = static_cast<Store*>(obj);
            InvokeAllObserver(store->GetName(), store->GetState() == 0 ? true : false);
            obj->Recycle();
        }
    }
};

// ***************

KF_RESULT KFAPI KFCreateAttributes(IKFAttributes** ppAttributes)
{
    if (ppAttributes == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) Attributes();
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    if (!result->Initialize(true)) {
        result->Recycle();
        return KF_INIT_ERROR;
    }

    *ppAttributes = result;
    return KF_OK;
}

KF_RESULT KFAPI KFCreateAttributesWithoutObserver(IKFAttributes** ppAttributes)
{
    if (ppAttributes == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) Attributes();
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    if (!result->Initialize(true)) {
        result->Recycle();
        return KF_INIT_ERROR;
    }

    *ppAttributes = result;
    return KF_OK;
}