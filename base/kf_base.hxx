#ifndef __KF_BASE_H
#define __KF_BASE_H

#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdarg>

#include <math.h>
#include <string.h>
#include <memory.h>

#include "kf_types.hxx"

#ifndef KFAPI
#ifdef _MSC_VER
#define KFAPI __cdecl
#else
#define KFAPI
#endif
#endif

#ifdef _MSC_VER
#include <intrin.h>
#define _KFRefInc(x) _InterlockedIncrement((x))
#define _KFRefDec(x) _InterlockedDecrement((x))
#else
#define _KFRefInc(x) (__sync_fetch_and_add((x),1) + 1)
#define _KFRefDec(x) __sync_sub_and_fetch((x),1)
#endif

typedef const char* KIID;
#ifdef _MSC_VER
typedef long KREF;
#else
typedef int KREF;
#endif

#define _KF_LOCK_INC(ptr) _KFRefInc(x)
#define _KF_LOCK_DEC(ptr) _KFRefDec(x)
#ifdef _MSC_VER
#define _KF_LOCK_CAS(ptr, equal, newvalue) _InterlockedCompareExchange(ptr, newvalue, equal)
#define _KF_LOCK_SWAP(ptr, newvalue) _InterlockedExchange(ptr, newvalue)
#else
#define _KF_LOCK_CAS(ptr, equal, newvalue) __sync_val_compare_and_swap(ptr, equal, newvalue)
#define _KF_LOCK_SWAP(ptr, newvalue) __sync_lock_test_and_set(ptr, newvalue); __sync_synchronize()
#endif

#define KF_ALLOC_ALIGNED(x) ((((x) >> 2) << 2) + 8) //4bytes.
#define KF_ARRAY_COUNT(ary) (sizeof(ary) / sizeof(ary[0]))

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_BASE_OBJECT       "kf_iid_bo"
#define _KF_INTERFACE_ID_DUMMY_OBJECT      "kf_iid_dummy_object"
#define _KF_INTERFACE_ID_OBJECT_FACTORY    "kf_iid_object_factory"
#define _KF_INTERFACE_ID_OBJECT_READWRITE  "kf_iid_object_readwrite"
#else
#define _KF_INTERFACE_ID_BASE_OBJECT       "0000000000000000C000000000000046"
#define _KF_INTERFACE_ID_DUMMY_OBJECT      "0000000100000000C000000000000046"
#define _KF_INTERFACE_ID_OBJECT_FACTORY    "0000000200000000C000000000000046"
#define _KF_INTERFACE_ID_OBJECT_READWRITE  "0000000300000000C000000000000046"
#endif
#define _KF_INTERFACE_ID_UNKNOWN _KF_INTERFACE_ID_BASE_OBJECT

#define _KFInterfaceIdEqual(id0, id1) (!strcmp(id0, id1))

struct IKFBaseObject
{
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv) = 0;
    virtual KREF Retain() = 0;
    virtual KREF Recycle() = 0;
};
typedef IKFBaseObject IKFUnknown;

struct IKFDummyObject : public IKFBaseObject
{
    virtual KF_UINT32 GetDummyTypeId() = 0;
};

struct IKFObjectFactory : public IKFBaseObject
{
    virtual KF_RESULT CreateObject(KIID interface_id, void** ppv) = 0;
};

struct IKFObjectReadWrite : public IKFBaseObject
{
    virtual int GetStreamLength() = 0;
    virtual KF_RESULT Read(KF_UINT8* buffer, int* length) = 0;
    virtual KF_RESULT Write(KF_UINT8* buffer, int length) = 0;
};

KF_RESULT KFAPI KFCreateDummyObject(KF_UINT32 dummyId, IKFDummyObject** dummyObject);
KF_RESULT KFAPI KFIsDummyObject(IKFBaseObject* object);

#define KF_SAFE_RELEASE(object) \
    if (object != nullptr) { object->Recycle(); object = nullptr; }
#define KF_DISALLOW_COPY_AND_ASSIGN(class_name) \
    class_name(const class_name&) = delete;      \
    class_name& operator=(const class_name&) = delete;

// *** BaseObject Helper *** //

#define KFBaseAddRef(object) \
    (object)->Retain()
#define KFBaseRelease(object) \
    (object)->Recycle()
#define KFBaseGetInterface(object, interface_id, ret_ptr) \
    (object)->CastToInterface((interface_id), (void**)(ret_ptr))

inline bool KFBaseHasInterface(void* object, KIID interface_id)
{
    void* ptr = nullptr;
    reinterpret_cast<IKFBaseObject*>(object)->CastToInterface(interface_id, &ptr);
    if (ptr)
        reinterpret_cast<IKFBaseObject*>(ptr)->Recycle();
    return ptr != nullptr;
}

#define KFFactoryCreateObject(factory, interface_id, ret_ptr) \
    (factory)->CreateObject((interface_id), (void**)(ret_ptr))

// *** Implement Helper *** //

#define KF_IMPL_DECL_REFCOUNT \
    volatile KREF _ref_count

#define KF_IMPL_RETAIN(xref)    \
    KREF Retain()                \
    { return _KFRefInc(&xref); }
#define KF_IMPL_RECYCLE(xref)   \
    KREF Recycle()               \
    { KREF rc = _KFRefDec(&xref); \
    if (rc == 0) delete this; return rc; }

#define KF_IMPL_RETAIN_FUNC(xref) \
    return _KFRefInc(&xref)
#define KF_IMPL_RECYCLE_FUNC(xref) \
    KREF rc = _KFRefDec(&xref);    \
    if (rc == 0) delete this; return rc

#define KF_IMPL_CHECK_PARAM \
    if (interface_id == nullptr || ppv == nullptr) \
        return KF_INVALID_ARG

#endif //__KF_BASE_H