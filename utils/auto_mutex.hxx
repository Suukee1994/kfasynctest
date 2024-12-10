#ifndef __KF_UTIL__AUTO_MUTEX_H
#define __KF_UTIL__AUTO_MUTEX_H

#include <sys/kf_sys_platform.h>
#include <base/kf_base.hxx>

class KFMutex final
{
    void* _mutex;

public:
    KFMutex() throw() { _mutex = KFMutexCreate(0); }
    KFMutex(bool cond_var) throw() { _mutex = KFMutexCreate(cond_var ? 1 : 0); }
    ~KFMutex() throw() { KFMutexDestroy(_mutex); }

    KFMutex(const KFMutex&) = delete;
    KFMutex& operator=(const KFMutex&) = delete;

    KFMutex(KFMutex&&) = delete;
    KFMutex& operator=(KFMutex&&) = delete;

public:
    void Lock() throw() { KFMutexLock(_mutex); }
    void Unlock() throw() { KFMutexUnlock(_mutex); }
    
    void* Get() throw() { return _mutex; }

public:
    class AutoLock final
    {
        KFMutex* _mutex;

    public:
        AutoLock() throw() : _mutex(nullptr) {}
        explicit AutoLock(KFMutex* m) throw() : _mutex(m) { if (_mutex) _mutex->Lock(); }
        explicit AutoLock(KFMutex& m) throw() : _mutex(&m) { _mutex->Lock(); }
        explicit AutoLock(const KFMutex& m) throw() : _mutex(const_cast<KFMutex*>(&m)) { _mutex->Lock(); }
        ~AutoLock() throw() { if (_mutex) _mutex->Unlock(); }

        KFMutex* GetMutex() throw() { return _mutex; }

        void Attach(KFMutex* m) throw() { Detach(); _mutex = m; if (_mutex) _mutex->Lock(); }
        void Detach() throw() { if (_mutex) _mutex->Unlock(); _mutex = nullptr; }

        void Clear() throw() { _mutex = nullptr; }

    public:
        AutoLock(const AutoLock&) = delete;
        AutoLock& operator=(const AutoLock&) = delete;

        AutoLock(AutoLock&&) = delete;
        AutoLock& operator=(AutoLock&&) = delete;

        void* operator new(std::size_t) = delete;
        void* operator new[](std::size_t) = delete;
    };

    class AutoLockReverse final
    {
        KFMutex* _mutex;

    public:
        AutoLockReverse() throw() : _mutex(nullptr) {}
        explicit AutoLockReverse(KFMutex* m) throw() : _mutex(m) { if (_mutex) _mutex->Unlock(); }
        explicit AutoLockReverse(KFMutex& m) throw() : _mutex(&m) { _mutex->Unlock(); }
        explicit AutoLockReverse(const KFMutex& m) throw() : _mutex(const_cast<KFMutex*>(&m)) { _mutex->Unlock(); }
        ~AutoLockReverse() throw() { if (_mutex) _mutex->Lock(); }

        KFMutex* GetMutex() throw() { return _mutex; }

        void Attach(KFMutex* m) throw() { Detach(); _mutex = m; if (_mutex) _mutex->Unlock(); }
        void Detach() throw() { if (_mutex) _mutex->Lock(); _mutex = nullptr; }

        void Clear() throw() { _mutex = nullptr; }
    
    public:
        AutoLockReverse(const AutoLockReverse&) = delete;
        AutoLockReverse& operator=(const AutoLockReverse&) = delete;

        AutoLockReverse(AutoLockReverse&&) = delete;
        AutoLockReverse& operator=(AutoLockReverse&&) = delete;

        void* operator new(std::size_t) = delete;
        void* operator new[](std::size_t) = delete;
    };
};

#endif //__KF_UTIL_AUTO_MUTEX_H