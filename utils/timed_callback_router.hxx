#ifndef __KF_UTIL__TIMED_CALLBACK_ROUTER_H
#define __KF_UTIL__TIMED_CALLBACK_ROUTER_H

#include <async/kf_timed_event.hxx>

template<typename Class>
class TimedCallbackRouter : public IKFTimedEventCallback
{
public:
    typedef void (Class::*InvokeTo)(IKFTimedEventState* state, IKFTimedEventQueue* queue);
    
public:
    TimedCallbackRouter() throw() : _outer(nullptr), _callback(nullptr) {}
    TimedCallbackRouter(Class* outer, InvokeTo cb) throw() : _outer(outer), _callback(cb) {}
    ~TimedCallbackRouter() throw() {}

    KF_DISALLOW_COPY_AND_ASSIGN(TimedCallbackRouter)
    
public:
    virtual KREF Retain() { return _outer->Retain(); }
    virtual KREF Recycle() { return _outer->Recycle(); }
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_TIMED_EVENT_CALLBACK)) {
            *ppv = static_cast<IKFTimedEventCallback*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }
    
    inline void SetCallback(Class* outer, InvokeTo cb) throw()
    { _outer = outer; _callback = cb; }
    
private:
    virtual void Invoke(IKFTimedEventState* state, IKFBaseObject* queue)
    {
        if (_outer != nullptr && _callback != nullptr)
            (_outer->*_callback)(state, reinterpret_cast<IKFTimedEventQueue*>(queue));
    }
    
private:
    Class* _outer;
    InvokeTo _callback;
};

#endif //__KF_UTIL__TIMED_CALLBACK_ROUTER_H