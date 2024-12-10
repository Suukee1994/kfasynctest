#ifndef __KF_ASYNC__ASYNC_ABSTRACT_H
#define __KF_ASYNC__ASYNC_ABSTRACT_H

#include <base/kf_base.hxx>

typedef void* KASYNCOBJECT;

#ifndef KF_INTERFACE_ID_USE_GUID
#define _KF_INTERFACE_ID_ASYNC_CALLBACK "kf_iid_async_callback"
#define _KF_INTERFACE_ID_ASYNC_RESULT "kf_iid_async_result"
#else
#define _KF_INTERFACE_ID_ASYNC_CALLBACK "1D14A9A529274CE3AD1AB69368B24132"
#define _KF_INTERFACE_ID_ASYNC_RESULT "8092C9DB2A414C43AD6496B82F50C0DC"
#endif

// ***** Async Interfaces ***** //

struct IKFAsyncResult : public IKFBaseObject
{
    virtual void SetResult(KF_RESULT result) = 0;
    virtual void SetObject(IKFBaseObject* pObject) = 0;

    virtual KF_RESULT GetResult() = 0;
    virtual KF_RESULT GetState(IKFBaseObject** ppState) = 0;
    virtual KF_RESULT GetObject(IKFBaseObject** ppObject) = 0;
    virtual IKFBaseObject* GetStateNoRef() = 0;
    virtual IKFBaseObject* GetObjectNoRef() = 0;
};

struct IKFAsyncCallback : public IKFBaseObject
{
    virtual void Execute(IKFAsyncResult* pResult) = 0; //实现这个接口，工作队列会去执行这个方法
};

// ***** Async Functions ***** //

#define KF_ASYNC_WORKER_THREADS_INFINITE           -1 //在 KFAsyncCreateWorker 的时候打开 parallel 选项才可使用

#define KF_ASYNC_GLOBAL_WORKER_SINGLE_THREAD       ((void*)1)
#define KF_ASYNC_GLOBAL_WORKER_MULTI_THREAD        ((void*)2)

//初始化异步核心（线程安全）
bool KFAPI KFAsyncStartup();
bool KFAPI KFAsyncLockRef(); // -> 自动做KFAsyncStartup
//销毁异步核心资源（线程安全）
void KFAPI KFAsyncShutdown();
void KFAPI KFAsyncUnlockRef(); // -> 等于KFAsyncShutdown

//创建一个异步队列工作者对象（线程不安全）
KF_RESULT KFAPI KFAsyncCreateWorker(bool parallel, int max_threads, KASYNCOBJECT* asyncObject, const char* worker_name = nullptr);
//销毁一个异步队列工作者对象（注意：这个函数返回后，工作者的线程组并没有立即全部退出，但是KASYNCOBJECT已经无效。）（线程不安全）
KF_RESULT KFAPI KFAsyncDestroyWorker(KASYNCOBJECT worker);
//取得最大的 Worker 的线程总数（线程不安全）
KF_RESULT KFAPI KFAsyncGetWorkerMaxThreads(KASYNCOBJECT worker, int* threads);
//取得 Worker 的名称，如果有的话（线程不安全）
KF_RESULT KFAPI KFAsyncGetWorkerName(KASYNCOBJECT worker, char* worker_name, int* name_len);

//根据用户提供的 Callback 组件，创建一个可供提交到异步工作者中的执行体对象 (IKFAsyncResult)
KF_RESULT KFAPI KFAsyncCreateResult(IKFAsyncCallback* callback, IKFBaseObject* state, IKFBaseObject* object, IKFAsyncResult** asyncResult);
//提交执行体对象 IKFAsyncResult 到工作队列中执行（会保证顺序遵循FIFO）
KF_RESULT KFAPI KFAsyncPutWorkItemEx(KASYNCOBJECT worker, IKFAsyncResult* result);
//直接根据 Callback 提交到工作队列中执行，内部会自动创建 IKFAsyncResult 对象
KF_RESULT KFAPI KFAsyncPutWorkItem(KASYNCOBJECT worker, IKFAsyncCallback* callback, IKFBaseObject* state);
//异步调用一个 Callback，这个调用会分发到默认的工作队列中执行（KFAsyncStartup 后创建的线程组）
KF_RESULT KFAPI KFAsyncInvokeCallback(IKFAsyncResult* result);

//不推荐使用的两个方法（可能有bug）
KF_RESULT KFAPI KFAsyncWorkerLockRef(KASYNCOBJECT worker);
KF_RESULT KFAPI KFAsyncWorkerUnlockRef(KASYNCOBJECT worker);

// ***** Async Helpers ***** //

struct KFAsyncInitializer
{
    KFAsyncInitializer() { KFAsyncStartup(); }
    ~KFAsyncInitializer() { KFAsyncShutdown(); }

    KF_DISALLOW_COPY_AND_ASSIGN(KFAsyncInitializer)
};

struct KFAsyncLocker
{
    KFAsyncLocker() { KFAsyncLockRef(); }
    ~KFAsyncLocker() { KFAsyncUnlockRef(); }

    KF_DISALLOW_COPY_AND_ASSIGN(KFAsyncLocker)
};

struct KFAsyncWorker
{
    KFAsyncWorker() : _worker(nullptr) {}
    ~KFAsyncWorker() { if (_worker) KFAsyncDestroyWorker(_worker); }

    KF_DISALLOW_COPY_AND_ASSIGN(KFAsyncWorker)

    KASYNCOBJECT* operator&() throw() { return &_worker; }
    inline KASYNCOBJECT Get() const throw() { return _worker; }
    inline void Destroy() throw() { if (_worker) KFAsyncDestroyWorker(_worker); Detach(); }

    inline KF_RESULT PutWorkItem(IKFAsyncCallback* callback) throw()
    { return KFAsyncPutWorkItem(_worker, callback, nullptr); }
    inline KF_RESULT PutWorkItem(IKFAsyncCallback* callback, IKFBaseObject* state) throw()
    { return KFAsyncPutWorkItem(_worker, callback, state); }

    inline void Attach(KASYNCOBJECT worker) throw() { _worker = worker; }
    inline void Detach() throw() { _worker = nullptr; }

private:
    KASYNCOBJECT _worker;
};

#endif //__KF_ASYNC__ASYNC_ABSTRACT_H