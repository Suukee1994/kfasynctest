#include <async/kf_async_internal.hxx>
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>

class MFCallback : public IMFAsyncCallback
{
    ULONG RefCount;
    IKFAsyncResult* KFCallback;

public:
    static MFCallback* MFCallbackCreateFromKFCallback(IKFAsyncResult* callback) throw()
    { return new(std::nothrow) MFCallback(callback); }

private:
    MFCallback() = delete;
    MFCallback(IKFAsyncResult* callback) throw() : RefCount(1)
    { KFCallback = callback; KFCallback->Retain(); }
    virtual ~MFCallback() throw() { KFCallback->Recycle(); }

public:
    STDMETHODIMP_(ULONG) AddRef()
    { return InterlockedIncrement(&RefCount); }
    STDMETHODIMP_(ULONG) Release()
    { auto rc = InterlockedDecrement(&RefCount); if (rc == 0) delete this; return rc; }
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
    {
        if (ppv == nullptr)
            return E_POINTER;
        if (iid == IID_IUnknown ||
            iid == IID_IMFAsyncCallback) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

public:
    STDMETHODIMP GetParameters(DWORD*,DWORD*) { return E_NOTIMPL; }
    STDMETHODIMP Invoke(IMFAsyncResult*)
    {
        IKFAsyncResult_I* result;
        if (KF_SUCCEEDED(KFBaseGetInterface(KFCallback, _INTERNAL_KF_INTERFACE_ID_ASYNC_RESULT, &result))) {
            IKFAsyncCallback* callback = nullptr;
            result->GetCallback(&callback);
            result->Recycle();
            callback->Execute(KFCallback);
            callback->Recycle();
        }
        return S_OK;
    }
};

// ***************

struct MFWorkerObject
{
    KREF RefCount;
    DWORD WorkQueue;
    char* Name;
};

bool KFAsyncMFStartup() { return SUCCEEDED(MFStartup(MF_VERSION)); }
void KFAsyncMFShutdown() { MFShutdown(); }

KF_RESULT KFAsyncCreateWorker_Win32_MF(bool parallel, int max_threads, KASYNCOBJECT* asyncObject, const char* worker_name)
{
    UNREFERENCED_PARAMETER(parallel);
    UNREFERENCED_PARAMETER(max_threads);

    if (asyncObject == nullptr)
        return KF_INVALID_PTR;

    MFWorkerObject object = {};
    auto hr = MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, &object.WorkQueue);
    if (FAILED(hr))
        return KF_ABORT;

    object.RefCount = 1;
    if (worker_name)
        object.Name = strdup(worker_name);

    auto p = malloc(sizeof(MFWorkerObject));
    if (p == nullptr) {
        MFUnlockWorkQueue(object.WorkQueue);
        return KF_OUT_OF_MEMORY;
    }

    memcpy(p, &object, sizeof(MFWorkerObject));
    *asyncObject = p;

    MFLockPlatform();
    return KF_OK;
}

KF_RESULT KFAsyncDestroyWorker_Win32_MF(KASYNCOBJECT worker)
{
    if (worker == nullptr)
        return KF_INVALID_ARG;
    
    auto my = reinterpret_cast<MFWorkerObject*>(worker);
    KREF rc = _KFRefDec(&my->RefCount);
    if (rc == 0) {
        MFUnlockWorkQueue(my->WorkQueue);
        MFUnlockPlatform();

        if (my->Name)
            free(my->Name);
        free(my);
    }
    return KF_OK;
}

KF_RESULT KFAsyncWorkerLockRef_Win32_MF(KASYNCOBJECT worker)
{
    if (worker == nullptr)
        return KF_INVALID_ARG;

    auto my = reinterpret_cast<MFWorkerObject*>(worker);
    _KFRefInc(&my->RefCount);
    return KF_OK;
}

KF_RESULT KFAsyncWorkerUnlockRef_Win32_MF(KASYNCOBJECT worker)
{
    return KFAsyncDestroyWorker_Win32_MF(worker);
}

KF_RESULT KFAsyncPutWorkItemEx_Win32_MF(KASYNCOBJECT worker, IKFAsyncResult* result)
{
    if (worker == nullptr || result == nullptr)
        return KF_INVALID_ARG;

    auto callback = MFCallback::MFCallbackCreateFromKFCallback(result);
    if (callback == nullptr)
        return KF_OUT_OF_MEMORY;
    
    auto my = reinterpret_cast<MFWorkerObject*>(worker);
    DWORD selector = 0;
    if (worker == KF_ASYNC_GLOBAL_WORKER_SINGLE_THREAD)
        selector = MFASYNC_CALLBACK_QUEUE_STANDARD;
    else if (worker == KF_ASYNC_GLOBAL_WORKER_MULTI_THREAD)
        selector = MFASYNC_CALLBACK_QUEUE_MULTITHREADED;
    else
        selector = my->WorkQueue;

    auto hr = MFPutWorkItem(selector, callback, nullptr);
    callback->Release();

    return SUCCEEDED(hr) ? KF_OK : KF_ERROR;
}

KF_RESULT KFAsyncPutWorkItem_Win32_MF(KASYNCOBJECT worker, IKFAsyncCallback* callback, IKFBaseObject* state)
{
    if (worker == nullptr || callback == nullptr)
        return KF_INVALID_ARG;

    IKFAsyncResult* result = nullptr;
    auto r = KFAsyncCreateResult(callback, state, nullptr, &result);
    _KF_FAILED_RET(r);

    r = KFAsyncPutWorkItemEx_Win32_MF(worker, result);
    result->Recycle();
    return r;
}

KF_RESULT KFAsyncInvokeCallback_Win32_MF(IKFAsyncResult* result)
{
    if (result == nullptr)
        return KF_INVALID_ARG;

    auto callback = MFCallback::MFCallbackCreateFromKFCallback(result);
    if (callback == nullptr)
        return KF_OUT_OF_MEMORY;

    IMFAsyncResult* store = nullptr;
    auto hr = MFCreateAsyncResult(nullptr, callback, nullptr, &store);
    if (FAILED(hr)) {
        callback->Release();
        return KF_ABORT;
    }

    callback->Release();
    hr = MFInvokeCallback(store);
    store->Release();

    return SUCCEEDED(hr) ? KF_OK : KF_ERROR;
}