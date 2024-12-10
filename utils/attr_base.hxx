#ifndef __KF_UTIL__ATTR_BASE_H
#define __KF_UTIL__ATTR_BASE_H

#include <base/kf_attr.hxx>

template <class TBase>
class KFAttributesBase : public TBase
{
    IKFAttributes* InternalAttrs;
protected:
    KFAttributesBase() throw()
    { InternalAttrs = nullptr; }
    virtual ~KFAttributesBase() throw()
    { KF_SAFE_RELEASE(InternalAttrs); }
    
    KF_RESULT CreateAttributes()
    {
        if (InternalAttrs == nullptr)
            return KFCreateAttributesWithoutObserver(&InternalAttrs);
        return KF_OK;
    }
    KF_RESULT InitAttributes(IKFBaseObject* obj)
    {
        if (InternalAttrs == nullptr) {
            KF_SAFE_RELEASE(InternalAttrs);
        }
        return KFBaseGetInterface(obj, _KF_INTERFACE_ID_ATTRIBUTES, &InternalAttrs);
    }
    
    inline IKFAttributes* GetAttributes() { return InternalAttrs; }
    
public:
    virtual KF_ATTRIBUTE_TYPE GetItemType(const char* key)
    { return InternalAttrs->GetItemType(key); }
    virtual KF_RESULT GetItemName(int index, IKFBuffer** name)
    { return InternalAttrs->GetItemName(index, name); }
    virtual int GetItemCount()
    { return InternalAttrs->GetItemCount(); }
    
    virtual KF_RESULT GetUINT32(const char* key, KF_UINT32* value)
    { return InternalAttrs->GetUINT32(key, value); }
    virtual KF_RESULT GetUINT64(const char* key, KF_UINT64* value)
    { return InternalAttrs->GetUINT64(key, value); }
    virtual KF_RESULT GetDouble(const char* key, double* value)
    { return InternalAttrs->GetDouble(key, value); }
    
    virtual KF_RESULT GetStringLength(const char* key, KF_UINT32* len)
    { return InternalAttrs->GetStringLength(key, len); }
    virtual KF_RESULT GetString(const char* key, char* string, KF_UINT32 str_ptr_len)
    { return InternalAttrs->GetString(key, string, str_ptr_len); }
    virtual KF_RESULT GetStringAlloc(const char* key, IKFBuffer** buffer)
    { return InternalAttrs->GetStringAlloc(key, buffer); }
    
    virtual KF_RESULT GetBlobLength(const char* key, KF_UINT32* len)
    { return InternalAttrs->GetBlobLength(key, len); }
    virtual KF_RESULT GetBlob(const char* key, void* buf, KF_UINT32 buf_ptr_len)
    { return InternalAttrs->GetBlob(key, buf, buf_ptr_len); }
    virtual KF_RESULT GetBlobAlloc(const char* key, IKFBuffer** buffer)
    { return InternalAttrs->GetBlobAlloc(key, buffer); }
    
    virtual KF_RESULT SetUINT32(const char* key, KF_UINT32 value)
    { return InternalAttrs->SetUINT32(key, value); }
    virtual KF_RESULT SetUINT64(const char* key, KF_UINT64 value)
    { return InternalAttrs->SetUINT64(key, value); }
    virtual KF_RESULT SetDouble(const char* key, double value)
    { return InternalAttrs->SetDouble(key, value); }
    
    virtual KF_RESULT SetString(const char* key, const char* value)
    { return InternalAttrs->SetString(key, value); }
    virtual KF_RESULT SetBlob(const char* key, const void* buf, KF_UINT32 buf_size)
    { return InternalAttrs->SetBlob(key, buf, buf_size); }
    
    virtual KF_RESULT GetObject(const char* key, KIID iid, void** ppv)
    { return InternalAttrs->GetObject(key, iid, ppv); }
    virtual KF_RESULT SetObject(const char* key, IKFBaseObject* object)
    { return InternalAttrs->SetObject(key, object); }
    
    virtual KF_RESULT DeleteItem(const char* key)
    { return InternalAttrs->DeleteItem(key); }
    virtual KF_RESULT DeleteAllItems()
    { return InternalAttrs->DeleteAllItems(); }
    
    virtual KF_RESULT MatchItem(const char* key, IKFAttributes* other_attr)
    { return InternalAttrs->MatchItem(key, other_attr); }
    virtual KF_RESULT MatchAllItems(IKFAttributes* other_attr)
    { return InternalAttrs->MatchAllItems(other_attr); }
    
    virtual KF_RESULT HasItem(const char* key)
    { return InternalAttrs->HasItem(key); }
    virtual KF_RESULT CopyItem(const char* key, IKFAttributes* copyTo)
    { return InternalAttrs->CopyItem(key, copyTo); }
    virtual KF_RESULT CopyAllItems(IKFAttributes* copyTo)
    { return InternalAttrs->CopyAllItems(copyTo); }
    
    virtual KF_RESULT SaveToBuffer(IKFBuffer** buffer)
    { return InternalAttrs->SaveToBuffer(buffer); }
    virtual KF_RESULT LoadFromBuffer(IKFBuffer* buffer)
    { return InternalAttrs->LoadFromBuffer(buffer); }
    
    virtual KF_RESULT ChangeObserverThreadMode(KF_ATTRIBUTE_OBSERVER_THREAD_MODE)
    { return KF_NOT_IMPLEMENTED; }
    
    virtual KF_RESULT AddObserver(IKFAttributesObserver*)
    { return KF_NOT_IMPLEMENTED; }
    virtual KF_RESULT RemoveObserver(IKFAttributesObserver*)
    { return KF_NOT_IMPLEMENTED; }
    
public: //IKFObjectReadWrite
    virtual int GetStreamLength()
    { return InternalAttrs->GetStreamLength(); }
    virtual KF_RESULT Read(KF_UINT8* buffer, int* length)
    { return InternalAttrs->Read(buffer, length); }
    virtual KF_RESULT Write(KF_UINT8* buffer, int length)
    { return InternalAttrs->Write(buffer, length); }
};

#endif //__KF_UTIL__ATTR_BASE_H