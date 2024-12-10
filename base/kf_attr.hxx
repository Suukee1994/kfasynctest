#ifndef __KF_BASE__ATTR_H
#define __KF_BASE__ATTR_H

#include <base/kf_base.hxx>
#include <base/kf_buffer.hxx>

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_ATTRIBUTES "kf_iid_attributes"
#define _KF_INTERFACE_ID_ATTRIBUTES_OBSERVER "kf_iid_attributes_observer"
#else
#define _KF_INTERFACE_ID_ATTRIBUTES "1870E66918D3410B9003AF11C50EC7B1"
#define _KF_INTERFACE_ID_ATTRIBUTES_OBSERVER "7712CB769EA04D7F8D6B9718728FE1B5"
#endif

enum KF_ATTRIBUTE_TYPE
{
    KF_ATTR_INVALID = 0,
    KF_ATTR_UINT32,
    KF_ATTR_UINT64,
    KF_ATTR_DOUBLE,
    KF_ATTR_STRING,
    KF_ATTR_BINARY,
    KF_ATTR_OBJECT
};

enum KF_ATTRIBUTE_OBSERVER_THREAD_MODE
{
    KF_ATTR_OBSERVER_THREAD_DIRECT_CALL = 0, //同步调用
    KF_ATTR_OBSERVER_THREAD_ASYNC_POOL       //异步调用
};

struct IKFAttributesObserver : public IKFBaseObject
{
    virtual void OnAttributeChanged(const char* key) = 0;
    virtual void OnAttributeDeleted(const char* key) = 0;

    virtual void OnDestroyAttributes() = 0;
};

struct IKFAttributes : public IKFObjectReadWrite
{
    virtual KF_ATTRIBUTE_TYPE GetItemType(const char* key) = 0;
    virtual KF_RESULT GetItemName(int index, IKFBuffer** name) = 0;
    virtual int GetItemCount() = 0;

    virtual KF_RESULT GetUINT32(const char* key, KF_UINT32* value) = 0;
    virtual KF_RESULT GetUINT64(const char* key, KF_UINT64* value) = 0;
    virtual KF_RESULT GetDouble(const char* key, double* value) = 0;

    virtual KF_RESULT GetStringLength(const char* key, KF_UINT32* len) = 0;
    virtual KF_RESULT GetString(const char* key, char* string, KF_UINT32 str_ptr_len) = 0;
    virtual KF_RESULT GetStringAlloc(const char* key, IKFBuffer** buffer) = 0;

    virtual KF_RESULT GetBlobLength(const char* key, KF_UINT32* len) = 0;
    virtual KF_RESULT GetBlob(const char* key, void* buf, KF_UINT32 buf_ptr_len) = 0;
    virtual KF_RESULT GetBlobAlloc(const char* key, IKFBuffer** buffer) = 0;

    virtual KF_RESULT SetUINT32(const char* key, KF_UINT32 value) = 0;
    virtual KF_RESULT SetUINT64(const char* key, KF_UINT64 value) = 0;
    virtual KF_RESULT SetDouble(const char* key, double value) = 0;

    virtual KF_RESULT SetString(const char* key, const char* value) = 0;
    virtual KF_RESULT SetBlob(const char* key, const void* buf, KF_UINT32 buf_size) = 0;

    virtual KF_RESULT GetObject(const char* key, KIID iid, void** ppv) = 0;
    virtual KF_RESULT SetObject(const char* key, IKFBaseObject* object) = 0;

    virtual KF_RESULT DeleteItem(const char* key) = 0;
    virtual KF_RESULT DeleteAllItems() = 0;

    virtual KF_RESULT HasItem(const char* key) = 0;
    virtual KF_RESULT CopyItem(const char* key, IKFAttributes* copyTo) = 0;
    virtual KF_RESULT CopyAllItems(IKFAttributes* copyTo) = 0;

    virtual KF_RESULT MatchItem(const char* key, IKFAttributes* other_attr) = 0;
    virtual KF_RESULT MatchAllItems(IKFAttributes* other_attr) = 0;

    virtual KF_RESULT SaveToBuffer(IKFBuffer** buffer) = 0; //会跳过所有的 IKFBaseObject 对象
    virtual KF_RESULT LoadFromBuffer(IKFBuffer* buffer) = 0;

    virtual KF_RESULT ChangeObserverThreadMode(KF_ATTRIBUTE_OBSERVER_THREAD_MODE mode) = 0;

    virtual KF_RESULT AddObserver(IKFAttributesObserver* observer) = 0;
    virtual KF_RESULT RemoveObserver(IKFAttributesObserver* observer) = 0;
};

KF_RESULT KFAPI KFCreateAttributes(IKFAttributes** ppAttributes);
KF_RESULT KFAPI KFCreateAttributesWithoutObserver(IKFAttributes** ppAttributes);

// ***************

inline KF_UINT32 KF_Attr_Hi32(KF_UINT64 unPacked) { return (KF_UINT32)(unPacked >> 32); }
inline KF_UINT32 KF_Attr_Lo32(KF_UINT64 unPacked) { return (KF_UINT32)unPacked; }

inline KF_UINT64 KF_Attr_Pack2UINT32AsUINT64(KF_UINT32 unHigh, KF_UINT32 unLow)
{ return ((KF_UINT64)unHigh << 32) | unLow; }
inline void KF_Attr_Unpack2UINT32AsUINT64(KF_UINT64 unPacked, KF_UINT32* punHigh, KF_UINT32* punLow)
{ *punHigh = KF_Attr_Hi32(unPacked); *punLow = KF_Attr_Lo32(unPacked); }

inline KF_UINT64 KF_Attr_PackSize(KF_UINT32 unWidth, KF_UINT32 unHeight)
{ return KF_Attr_Pack2UINT32AsUINT64(unWidth, unHeight); }
inline void KF_Attr_UnpackSize(KF_UINT64 unPacked, KF_UINT32* punWidth, KF_UINT32* punHeight)
{ KF_Attr_Unpack2UINT32AsUINT64(unPacked, punWidth, punHeight); }

inline KF_UINT64 KF_Attr_PackRatio(KF_INT32 nNumerator, KF_UINT32 unDenominator)
{ return KF_Attr_Pack2UINT32AsUINT64((KF_UINT32)nNumerator, unDenominator); }
inline void KF_Attr_UnpackRatio(KF_UINT64 unPacked, KF_INT32* pnNumerator, KF_UINT32* punDenominator)
{ KF_Attr_Unpack2UINT32AsUINT64(unPacked, (KF_UINT32*)pnNumerator, punDenominator); }

// ***************

inline KF_UINT32 KFGetAttributeUINT32(IKFAttributes* attr, const char* key, KF_UINT32 nDefault)
{
    KF_UINT32 ret;
    if (KF_FAILED(attr->GetUINT32(key, &ret)))
        ret = nDefault;
    return ret;
}

inline KF_UINT64 KFGetAttributeUINT64(IKFAttributes* attr, const char* key, KF_UINT64 nDefault)
{
    KF_UINT64 ret;
    if (KF_FAILED(attr->GetUINT64(key, &ret)))
        ret = nDefault;
    return ret;
}

inline double KFGetAttributeDouble(IKFAttributes* attr, const char* key, double nDefault)
{
    double ret;
    if (KF_FAILED(attr->GetDouble(key, &ret)))
        ret = nDefault;
    return ret;
}

inline KF_UINT32 KFGetAttributeStringSize(IKFAttributes* attr, const char* key)
{
    KF_UINT32 len = 0;
    attr->GetStringLength(key, &len);
    return len;
}

inline KF_UINT32 KFGetAttributeBlobSize(IKFAttributes* attr, const char* key)
{
    KF_UINT32 len = 0;
    attr->GetBlobLength(key, &len);
    return len;
}

inline KF_RESULT KFGetAttribute2UINT32asUINT64(IKFAttributes* attr, const char* key, KF_UINT32* high32, KF_UINT32* low32)
{
    KF_UINT64 unpacked;
    KF_RESULT r = attr->GetUINT64(key, &unpacked);
    _KF_FAILED_RET(r);
    KF_Attr_Unpack2UINT32AsUINT64(unpacked, high32, low32);
    return r;
}

inline KF_RESULT KFSetAttribute2UINT32asUINT64(IKFAttributes* attr, const char* key, KF_UINT32 high32, KF_UINT32 low32)
{
    return attr->SetUINT64(key, KF_Attr_Pack2UINT32AsUINT64(high32, low32));
}

inline KF_RESULT KFGetAttributeRatio(IKFAttributes* attr, const char* key, KF_UINT32* num, KF_UINT32* den)
{ return KFGetAttribute2UINT32asUINT64(attr, key, num, den); }
inline KF_RESULT KFGetAttributeSize(IKFAttributes* attr, const char* key, KF_UINT32* width, KF_UINT32* height)
{ return KFGetAttribute2UINT32asUINT64(attr, key, width, height); }
inline KF_RESULT KFGetAttributePair(IKFAttributes* attr, const char* key, KF_UINT32* low, KF_UINT32* high)
{ return KFGetAttribute2UINT32asUINT64(attr, key, low, high); }

inline KF_RESULT KFSetAttributeRatio(IKFAttributes* attr, const char* key, KF_UINT32 num, KF_UINT32 den)
{ return KFSetAttribute2UINT32asUINT64(attr, key, num, den); }
inline KF_RESULT KFSetAttributeSize(IKFAttributes* attr, const char* key, KF_UINT32 width, KF_UINT32 height)
{ return KFSetAttribute2UINT32asUINT64(attr, key, width, height); }
inline KF_RESULT KFSetAttributePair(IKFAttributes* attr, const char* key, KF_UINT32 low, KF_UINT32 high)
{ return KFSetAttribute2UINT32asUINT64(attr, key, low, high); }

inline KF_RESULT KFSetAttributeIntPtr(IKFAttributes* attr, const char* key, void* ptr)
{ return attr->SetUINT64(key, reinterpret_cast<KF_UINT64>(ptr)); }
inline void* KFGetAttributeIntPtr(IKFAttributes* attr, const char* key)
{ return reinterpret_cast<void*>(KFGetAttributeUINT64(attr, key, KF_UINT64(NULL))); }

inline KF_RESULT KFSetAttributeBool(IKFAttributes* attr, const char* key, bool value)
{ return attr->SetUINT32(key, value ? KF_TRUE : KF_FALSE); }
inline bool KFGetAttributeBool(IKFAttributes* attr, const char* key)
{ return KFGetAttributeUINT32(attr, key, KF_FALSE) ? true : false; }

// ***************

template<typename T> inline KF_RESULT KFSetAttributeValue(IKFAttributes*,const char*,T) { return KF_ABORT; }
template<> inline KF_RESULT KFSetAttributeValue<int>(IKFAttributes* attr, const char* key, int value)
{ return attr->SetUINT32(key, KF_UINT32(value)); }
template<> inline KF_RESULT KFSetAttributeValue<unsigned>(IKFAttributes* attr, const char* key, unsigned value)
{ return attr->SetUINT32(key, value); }
template<> inline KF_RESULT KFSetAttributeValue<long long>(IKFAttributes* attr, const char* key, long long value)
{ return attr->SetUINT64(key, KF_UINT64(value)); }
template<> inline KF_RESULT KFSetAttributeValue<unsigned long long>(IKFAttributes* attr, const char* key, unsigned long long value)
{ return attr->SetUINT64(key, value); }
template<> inline KF_RESULT KFSetAttributeValue<float>(IKFAttributes* attr, const char* key, float value)
{ return attr->SetDouble(key, double(value)); }
template<> inline KF_RESULT KFSetAttributeValue<double>(IKFAttributes* attr, const char* key, double value)
{ return attr->SetDouble(key, value); }
template<> inline KF_RESULT KFSetAttributeValue<const char*>(IKFAttributes* attr, const char* key, const char* value)
{ return attr->SetString(key, value); }
template<> inline KF_RESULT KFSetAttributeValue<IKFBaseObject*>(IKFAttributes* attr, const char* key, IKFBaseObject* value)
{ return attr->SetObject(key, value); }

// ***************

inline KF_RESULT KFSetAttributeNull(IKFAttributes* attr, const char* key) {
    IKFDummyObject* dummy = nullptr;
    KF_RESULT r = KFCreateDummyObject(0xFFFFFFFF, &dummy);
    _KF_FAILED_RET(r);
    r = attr->SetObject(key, dummy);
    dummy->Recycle();
    return r;
}

inline bool KFIsAttributeNull(IKFAttributes* attr, const char* key) {
    bool result = false;
    IKFBaseObject* obj = nullptr;
    attr->GetObject(key, _KF_INTERFACE_ID_DUMMY_OBJECT, (void**)&obj);
    if (obj != nullptr) {
        result = true;
        obj->Recycle();
    }
    return result;
}

#endif //__KF_BASE__ATTR_H