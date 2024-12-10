#include <base/kf_base.hxx>
#include <base/kf_buffer_internal.hxx>
#include <base/kf_array_list.hxx>
#include <base/kf_fast_list.hxx>

class MemoryBuffer : public IKFBuffer_I
{
    KF_IMPL_DECL_REFCOUNT;

    void *_buf, *_buf_fixed;
    int _max_len, _cur_len;

    KFBufferReleaseCallback _release_callback;
    void* _userdata;

    int _platform_type;

public:
    MemoryBuffer() throw() : _ref_count(1),
    _buf(nullptr), _buf_fixed(nullptr),
    _max_len(-1), _cur_len(0)
    { _release_callback = nullptr; _userdata = nullptr; _platform_type = KF_BUFFER_PLATFORM_TYPE_GENERIC; }
    virtual ~MemoryBuffer() throw()
    { if (_buf) free(_buf); if (_buf_fixed && _release_callback) _release_callback(this, _buf_fixed, _userdata); }

    bool Prepare(int max_len, bool fast = false) throw()
    {
        _max_len = max_len;
        _buf = malloc(KF_ALLOC_ALIGNED(max_len));
        if (_buf && !fast)
            memset(_buf, 0, KF_ALLOC_ALIGNED(max_len));
        return _buf != nullptr;
    }
    bool PrepareFixed(const void* ptr, int ptr_len) throw()
    {
        _max_len = _cur_len = ptr_len;
        _buf_fixed = const_cast<void*>(ptr);
        return true;
    }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BUFFER) ||
            _KFInterfaceIdEqual(interface_id, _INTERNAL_KF_INTERFACE_ID_BUFFER)) {
            *ppv = static_cast<IKFBuffer_I*>(this);
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
    virtual unsigned char* GetAddress() { return (unsigned char*)(_buf ? _buf : _buf_fixed); }
    virtual bool IsMemoryFixed() { return _buf_fixed != nullptr; }
    virtual void SetCurrentLength(int cur_len) { _cur_len = cur_len; }
    virtual int GetCurrentLength() { return _cur_len; }
    virtual int GetMaxLength() { return _max_len; }

    virtual int GetPlatformType() { return _platform_type; }
    virtual const char* GetPlatformName() { return nullptr; }
    virtual void* GetPlatformObject() { return nullptr; }
    virtual bool IsPlatformSpecified() { return (_platform_type != KF_BUFFER_PLATFORM_TYPE_GENERIC); }

public:
    virtual void SetPlatformType(int type) { _platform_type = type; }
    virtual void SetReleaseCallback(KFBufferReleaseCallback callback, void* userdata)
    { _release_callback = callback; _userdata = userdata; }
};

// ***************

KF_RESULT KFAPI KFCreateMemoryBuffer(int max_len, IKFBuffer** ppBuffer)
{
    if (max_len == 0 || ppBuffer == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) MemoryBuffer();
    if (result == nullptr || !result->Prepare(max_len)) {
        if (result)
            result->Recycle();
        return result ? KF_UNEXPECTED : KF_OUT_OF_MEMORY;
    }

    *ppBuffer = result;
    return KF_OK;
}

KF_RESULT KFAPI KFCreateMemoryBufferFast(int max_len, IKFBuffer** ppBuffer)
{
    if (max_len == 0 || ppBuffer == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) MemoryBuffer();
    if (result == nullptr || !result->Prepare(max_len, true)) {
        if (result)
            result->Recycle();
        return result ? KF_UNEXPECTED : KF_OUT_OF_MEMORY;
    }

    *ppBuffer = result;
    return KF_OK;
}

KF_RESULT KFAPI KFCreateMemoryBufferFixed(const void* ptr, int ptr_len, IKFBuffer** ppBuffer)
{
    if (ptr == nullptr || ptr_len == 0)
        return KF_INVALID_ARG;
    if (ppBuffer == nullptr)
        return KF_INVALID_PTR;

    auto result = new(std::nothrow) MemoryBuffer();
    if (result == nullptr)
        return KF_OUT_OF_MEMORY;

    result->PrepareFixed(ptr, ptr_len);
    *ppBuffer = result;
    return KF_OK;
}

KF_RESULT KFAPI KFCreateMemoryBufferFixedEx(const void* ptr, int ptr_len, IKFBuffer** ppBuffer, KFBufferReleaseCallback release_callback, void* userdata)
{
    IKFBuffer* buffer = nullptr;
    auto result = KFCreateMemoryBufferFixed(ptr, ptr_len, &buffer);
    _KF_FAILED_RET(result);

    if (release_callback) {
        IKFBuffer_I* internalObj = nullptr;
        KFBaseGetInterface(buffer, _INTERNAL_KF_INTERFACE_ID_BUFFER, &internalObj);
        internalObj->SetReleaseCallback(release_callback, userdata);
        internalObj->Recycle();
    }

    *ppBuffer = buffer;
    return KF_OK;
}

KF_RESULT KFAPI KFCreateMemoryBufferCopy(IKFBuffer* copy_buffer, int max_len, IKFBuffer** ppBuffer)
{
    if (copy_buffer == nullptr)
        return KF_INVALID_ARG;
    if (ppBuffer == nullptr)
        return KF_INVALID_PTR;

    int len = max_len;
    if (len == -1)
        len = copy_buffer->GetCurrentLength() + 1;
    if (len < copy_buffer->GetCurrentLength())
        return KF_INVALID_INPUT;

    IKFBuffer* buffer = nullptr;
    auto result = KFCreateMemoryBufferFast(len, &buffer);
    _KF_FAILED_RET(result);

    memcpy(buffer->GetAddress(), copy_buffer->GetAddress(), copy_buffer->GetCurrentLength());
    buffer->SetCurrentLength(copy_buffer->GetCurrentLength());

    *ppBuffer = buffer;
    return KF_OK;
}

KF_RESULT KFAPI KFCreateMemoryBufferString(const char* s, IKFBuffer** ppBuffer)
{
    if (s == nullptr)
        return KF_INVALID_ARG;
    int len = (int)strlen(s);
    if (len == 0)
        return KF_INVALID_INPUT;

    IKFBuffer* buffer;
    auto result = KFCreateMemoryBuffer(len + 1, &buffer);
    _KF_FAILED_RET(result);
    strcpy((char*)buffer->GetAddress(), s);
    buffer->SetCurrentLength(len);
    *ppBuffer = buffer;
    return KF_OK;
}

KF_RESULT KFAPI KFCreateMemoryBufferStringV(IKFBuffer** ppBuffer, const char* format, ...)
{
    if (ppBuffer == nullptr)
        return KF_INVALID_PTR;
    if (format == nullptr)
        return KF_INVALID_INPUT;

    IKFBuffer* buffer = nullptr;
    auto result = KFCreateMemoryBuffer(4096, &buffer);
    _KF_FAILED_RET(result);

    auto s = (char*)buffer->GetAddress();
    va_list args;
    va_start(args, format);
    vsnprintf(s, buffer->GetMaxLength(), format, args);
    va_end(args);
    buffer->SetCurrentLength(strlen(s));
    *ppBuffer = buffer;
    return KF_OK;
}

KF_RESULT KFAPI KFCopyMemoryBuffer(IKFBuffer* src, IKFBuffer* dst)
{
    if (src == nullptr || dst == nullptr)
        return KF_INVALID_ARG;

    if (src->GetMaxLength() > dst->GetMaxLength())
        return KF_INVALID_INPUT;

    dst->SetCurrentLength(src->GetCurrentLength());
    memcpy(dst->GetAddress(), src->GetAddress(), src->GetCurrentLength());
    return KF_OK;
}

KF_RESULT KFAPI KFMergeMemoryBuffer(IKFBuffer* buf1, IKFBuffer* buf2, IKFBuffer** mergedBuf)
{
    if (buf1 == nullptr && buf2 == nullptr)
        return KF_INVALID_ARG;
    if (mergedBuf == nullptr)
        return KF_INVALID_PTR;

    int s1 = (buf1 != nullptr ? buf1->GetCurrentLength() : 0);
    int s2 = (buf2 != nullptr ? buf2->GetCurrentLength() : 0);
    int newLen = s1 + s2;
    if (newLen == 0)
        return KF_INVALID_INPUT;

    IKFBuffer* buffer = nullptr;
    auto result = KFCreateMemoryBufferFast(newLen + 1, &buffer);
    _KF_FAILED_RET(result);

    if (s1 > 0)
        memcpy(buffer->GetAddress(), buf1->GetAddress(), s1);
    if (s2 > 0)
        memcpy(buffer->GetAddress() + s1, buf2->GetAddress(), s2);
    
    buffer->SetCurrentLength(newLen);
    *mergedBuf = buffer;
    return KF_OK;
}

KF_RESULT KFAPI KFMergeMemoryBufferUseListObject(IKFBaseObject* list, IKFBuffer** mergedBuf)
{
    if (list == nullptr)
        return KF_INVALID_ARG;
    if (mergedBuf == nullptr)
        return KF_INVALID_PTR;

    IKFArrayList* arrayList = nullptr;
    KFBaseGetInterface(list, _KF_INTERFACE_ID_OBJECT_ARRAY_LIST, &arrayList);
    if (arrayList == nullptr)
        return KF_NOT_IMPLEMENTED;

    IKFBuffer* buffer = nullptr;
    int count = arrayList->GetElementCount();
    if (count == 1) {
        KFGetArrayListElement<IKFBuffer>(arrayList, 0, _KF_INTERFACE_ID_BUFFER, &buffer);
    } else {
        IKFBuffer** bufList = (IKFBuffer**)KFAllocz((count + 1) * sizeof(void*));
        int totalSize = 0;
        for (int i = 0; i < count; i++) {
            if (KFGetArrayListElement<IKFBuffer>(arrayList, i, _KF_INTERFACE_ID_BUFFER, &buffer)) {
                *(bufList + i) = buffer;
                totalSize += buffer->GetCurrentLength();
            }
        }
        buffer = nullptr;
        if (totalSize > 0) {
            KFCreateMemoryBuffer(totalSize + 16, &buffer);
            if (buffer != nullptr) {
                char* data = KFGetBufferAddress<char*>(buffer);
                IKFBuffer* curBuf = nullptr;
                int curPos = 0;
                for (int i = 0; i < count; i++) {
                    curBuf = *(bufList + i);
                    if (curBuf != nullptr) {
                        int curLen = curBuf->GetCurrentLength();
                        if (curLen > 0) {
                            memcpy(data + curPos, KFGetBufferAddress<char*>(curBuf), curLen);
                            curPos += curLen;
                        }
                    }
                }
                buffer->SetCurrentLength(totalSize);
            }
        }
        for (int i = 0; i < count; i++) {
            if (*(bufList + i) != nullptr)
                (*(bufList + i))->Recycle();
        }
        KFFreep((void**)&bufList);
    }

    arrayList->Recycle();
    *mergedBuf = buffer;
    return buffer != nullptr ? KF_OK : KF_ABORT;
}