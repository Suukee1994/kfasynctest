#ifndef __KF_UTIL__AUTO_RWLOCK_H
#define __KF_UTIL__AUTO_RWLOCK_H

#include <sys/kf_sys_platform.h>
#include <base/kf_base.hxx>

class KFRWLock final
{
    void* _rwlock;

public:
    KFRWLock() throw() { _rwlock = KFRWLockCreate(); }
    ~KFRWLock() throw() { KFRWLockDestroy(_rwlock); }

    KFRWLock(const KFRWLock&) = delete;
    KFRWLock& operator=(const KFRWLock&) = delete;

    KFRWLock(KFRWLock&&) = delete;
    KFRWLock& operator=(KFRWLock&&) = delete;

public:
    void LockR() throw() { KFRWLockAcquireShared(_rwlock); }
    void UnlockR() throw() { KFRWLockReleaseShared(_rwlock); }

    void LockW() throw() { KFRWLockAcquireExclusive(_rwlock); }
    void UnlockW() throw() { KFRWLockReleaseExclusive(_rwlock); }

public:
    class AutoReadLock final
    {
        KFRWLock* _rwlock;

    public:
        AutoReadLock() throw() : _rwlock(nullptr) {}
        explicit AutoReadLock(KFRWLock* m) throw() : _rwlock(m) { _rwlock->LockR(); }
        explicit AutoReadLock(KFRWLock& m) throw() : _rwlock(&m) { _rwlock->LockR(); }
        ~AutoReadLock() throw() { if (_rwlock) _rwlock->UnlockR(); }

        void Attach(KFRWLock* m) throw() { Detach(); _rwlock = m; _rwlock->LockR(); }
        void Detach() throw() { if (_rwlock) _rwlock->UnlockR(); _rwlock = nullptr; }

        void Clear() throw() { _rwlock = nullptr; }

    public:
        AutoReadLock(const AutoReadLock&) = delete;
        AutoReadLock& operator=(const AutoReadLock&) = delete;

        AutoReadLock(AutoReadLock&&) = delete;
        AutoReadLock& operator=(AutoReadLock&&) = delete;

        void* operator new(std::size_t) = delete;
        void* operator new[](std::size_t) = delete;
    };


    class AutoWriteLock final
    {
        KFRWLock* _rwlock;

    public:
        AutoWriteLock() throw() : _rwlock(nullptr) {}
        explicit AutoWriteLock(KFRWLock* m) throw() : _rwlock(m) { _rwlock->LockW(); }
        explicit AutoWriteLock(KFRWLock& m) throw() : _rwlock(&m) { _rwlock->LockW(); }
        ~AutoWriteLock() throw() { if (_rwlock) _rwlock->UnlockW(); }

        void Attach(KFRWLock* m) throw() { Detach(); _rwlock = m; _rwlock->LockW(); }
        void Detach() throw() { if (_rwlock) _rwlock->UnlockW(); _rwlock = nullptr; }

        void Clear() throw() { _rwlock = nullptr; }

    public:
        AutoWriteLock(const AutoWriteLock&) = delete;
        AutoWriteLock& operator=(const AutoWriteLock&) = delete;

        AutoWriteLock(AutoWriteLock&&) = delete;
        AutoWriteLock& operator=(AutoWriteLock&&) = delete;

        void* operator new(std::size_t) = delete;
        void* operator new[](std::size_t) = delete;
    };
};

#endif //__KF_UTIL_AUTO_RWLOCK_H