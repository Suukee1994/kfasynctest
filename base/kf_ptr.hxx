#ifndef __KF_PTR_H
#define __KF_PTR_H

#include <base/kf_base.hxx>

template<typename Type>
class KFPtr final
{
    Type* _ptr;

    void InternalAddRef()
    {
        if (_ptr)
            _ptr->Retain();
    }
    void InternalRelease()
    {
        if (_ptr)
            _ptr->Recycle();
        _ptr = nullptr;
    }

public:
    KFPtr() throw() : _ptr(nullptr) {}
    KFPtr(Type* ptr) throw() : _ptr(ptr) { InternalAddRef(); }
    KFPtr(const KFPtr<Type>& other) throw() : _ptr(other._ptr) { InternalAddRef(); }
    KFPtr(KFPtr<Type>&& other) throw() //Move with C++11.
    {
        if (this != reinterpret_cast<KFPtr*>(&reinterpret_cast<unsigned char&>(other))) {
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
    }
    ~KFPtr() throw() { InternalRelease(); }

    KFPtr& operator=(Type* ptr) throw()
    {
        if (_ptr != ptr) {
            Type* old = _ptr;
            _ptr = ptr;
            InternalAddRef();
            if (old)
                old->Recycle();
        }
        return *this;
    }
    KFPtr& operator=(const KFPtr<Type>& other) throw()
    { return operator=(other._ptr); }
    KFPtr& operator=(KFPtr<Type>&& other) = delete;

    void Attach(Type* ptr) throw()
    {
        InternalRelease();
        _ptr = ptr;
    }
    Type* Detach() throw()
    {
        Type* const old = _ptr;
        _ptr = nullptr;
        return old;
    }

    operator Type*() const throw() { return _ptr; }
    Type& operator*() const throw() { return *_ptr; }
    Type* operator->() const throw() { return _ptr; }
    Type** operator&() throw() { return ResetAndGetAddressOf(); }

    bool operator==(const KFPtr<Type>& other) throw()
    { return (_ptr == other._ptr); }
    bool operator==(Type* ptr) throw()
    { return (_ptr == ptr); }

    bool operator!=(const KFPtr<Type>& other) throw()
    { return !(operator==(other)); }
    bool operator!=(Type* ptr) throw()
    { return !(operator==(ptr)); }

    Type* Get() const throw() { return _ptr; }

    KF_RESULT CopyTo(Type** ptr) const throw() { InternalRelease(); *ptr = _ptr; return KF_OK; }
    KF_RESULT CopyTo(KIID interface_id, void** ppv) const throw()
    { return _ptr->CastToInterface(interface_id, ppv); }

    Type* const* GetAddressOf() const throw() { return &_ptr; }
    Type** GetAddressOf() throw() { return &_ptr; }

    void Reset() throw() { InternalRelease(); }
    Type** ResetAndGetAddressOf() throw() { InternalRelease(); return &_ptr; }
};

template<typename T>
inline static T* VarPtr(T& Object) { return &Object; }

#endif //__KF_PTR_H