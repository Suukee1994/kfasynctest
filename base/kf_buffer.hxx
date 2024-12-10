#ifndef __KF_BASE__BUFFER_H
#define __KF_BASE__BUFFER_H

#include <base/kf_base.hxx>

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_BUFFER "kf_iid_buffer"
#else
#define _KF_INTERFACE_ID_BUFFER "DEF410B560524DAE8645FAE8150CA602"
#endif
struct IKFBuffer : public IKFBaseObject
{
    virtual unsigned char* GetAddress() = 0;
    virtual bool IsMemoryFixed() = 0;
    virtual void SetCurrentLength(int cur_len) = 0;
    virtual int GetCurrentLength() = 0;
    virtual int GetMaxLength() = 0;

    virtual int GetPlatformType() = 0;
    virtual const char* GetPlatformName() = 0;
    virtual void* GetPlatformObject() = 0;
    virtual bool IsPlatformSpecified() = 0;
};

#define KF_BUFFER_PLATFORM_TYPE_UNKNOWN -1
#define KF_BUFFER_PLATFORM_TYPE_GENERIC  0

typedef void (*KFBufferReleaseCallback)(IKFBuffer* object, void* ptr, void* userdata);

KF_RESULT KFAPI KFCreateMemoryBuffer(int max_len, IKFBuffer** ppBuffer);
KF_RESULT KFAPI KFCreateMemoryBufferFast(int max_len, IKFBuffer** ppBuffer);
KF_RESULT KFAPI KFCreateMemoryBufferFixed(const void* ptr, int ptr_len, IKFBuffer** ppBuffer);
KF_RESULT KFAPI KFCreateMemoryBufferFixedEx(const void* ptr, int ptr_len, IKFBuffer** ppBuffer, KFBufferReleaseCallback release_callback, void* userdata);
KF_RESULT KFAPI KFCreateMemoryBufferCopy(IKFBuffer* copy_buffer, int max_len, IKFBuffer** ppBuffer);
KF_RESULT KFAPI KFCreateMemoryBufferString(const char* s, IKFBuffer** ppBuffer);
KF_RESULT KFAPI KFCreateMemoryBufferStringV(IKFBuffer** ppBuffer, const char* format, ...);

KF_RESULT KFAPI KFCopyMemoryBuffer(IKFBuffer* src, IKFBuffer* dst);
KF_RESULT KFAPI KFMergeMemoryBuffer(IKFBuffer* buf1, IKFBuffer* buf2, IKFBuffer** mergedBuf);
KF_RESULT KFAPI KFMergeMemoryBufferUseListObject(IKFBaseObject* list, IKFBuffer** mergedBuf);

template<typename ResultType>
inline ResultType KFGetBufferAddress(IKFBuffer* buffer)
{ return reinterpret_cast<ResultType>(buffer->GetAddress()); }
template<typename ResultType>
inline ResultType KFGetBufferAddressSafe(IKFBuffer* buffer)
{ if (buffer == nullptr) return reinterpret_cast<ResultType>(NULL);
  return KFGetBufferAddress<ResultType>(buffer); }

// ***************

inline static void* KFAllocz(size_t size)
{
    void* ptr = malloc(size);
    if (ptr)
        memset(ptr, 0, size);
    return ptr;
}
inline static void KFFreep(void** ptr)
{
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

#endif //__KF_BASE__BUFFER_H