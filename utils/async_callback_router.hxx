#ifndef __KF_UTIL__ASYNC_CALLBACK_ROUTER_H
#define __KF_UTIL__ASYNC_CALLBACK_ROUTER_H

#include <async/kf_async_abstract.hxx>

template<class Class>
class AsyncCallbackRouter : public IKFAsyncCallback
{
public:
    typedef void (Class::*InvokeTo)(IKFAsyncResult*);

public:
    AsyncCallbackRouter() throw() : _outer(nullptr), _callback(nullptr) {}
    AsyncCallbackRouter(Class* outer, InvokeTo cb) throw() : _outer(outer), _callback(cb) {}
    ~AsyncCallbackRouter() throw() {}

    KF_DISALLOW_COPY_AND_ASSIGN(AsyncCallbackRouter)

public:
    virtual KREF Retain() { return _outer->Retain(); }
    virtual KREF Recycle() { return _outer->Recycle(); }
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_ASYNC_CALLBACK)) {
            *ppv = static_cast<IKFAsyncCallback*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }

    inline void SetCallback(Class* outer, InvokeTo cb) throw()
    { _outer = outer; _callback = cb; }

private:
    virtual void Execute(IKFAsyncResult* pResult)
    {
        if (_outer != nullptr && _callback != nullptr)
            (_outer->*_callback)(pResult);
    }

private:
    Class* _outer;
    InvokeTo _callback;
};

#endif //__KF_UTIL__ASYNC_CALLBACK_ROUTER_H