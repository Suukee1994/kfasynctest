#include <utils/auto_mutex.hxx>
#include <base/kf_base.hxx>
#include <base/kf_log.hxx>
#include <base/kf_array_list.hxx>
#include <async/kf_thread_object.hxx>
#include <async/kf_async_internal.hxx>

#define KF_LOG_TAG_STR "kf_async_core.cxx"

#ifdef _MSC_VER
#pragma warning(disable:4127)
#pragma warning(disable:4100)
#ifdef KF_WIN_MF_WORKQUEUE
bool KFAsyncMFStartup();
void KFAsyncMFShutdown();
KF_RESULT KFAsyncCreateWorker_Win32_MF(bool, int, KASYNCOBJECT*, const char*);
KF_RESULT KFAsyncDestroyWorker_Win32_MF(KASYNCOBJECT);
KF_RESULT KFAsyncWorkerLockRef_Win32_MF(KASYNCOBJECT);
KF_RESULT KFAsyncWorkerUnlockRef_Win32_MF(KASYNCOBJECT);
KF_RESULT KFAsyncPutWorkItemEx_Win32_MF(KASYNCOBJECT, IKFAsyncResult*);
KF_RESULT KFAsyncPutWorkItem_Win32_MF(KASYNCOBJECT, IKFAsyncCallback*, IKFBaseObject*);
KF_RESULT KFAsyncInvokeCallback_Win32_MF(IKFAsyncResult*);
#endif
#endif

#define WORKER_EXIT_TIMEOUT (60 * 1000)
#define MAX_OVERFLOW_THREAD_COUNT 10

class WorkItem : public IKFAsyncWorkItem_I
{
    KF_IMPL_DECL_REFCOUNT;

    IKFAsyncWorkItem_I::WorkItemState _state;
    IKFAsyncResult_I* _result;

public:
    WorkItem(IKFAsyncWorkItem_I::WorkItemState state, IKFAsyncResult_I* result) throw() : _ref_count(1), _state(state)
    { if (result) result->Retain(); _result = result; }
    virtual ~WorkItem() throw()
    { if (_result) _result->Recycle(); }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _INTERNAL_KF_INTERFACE_ID_ASYNC_WORK_ITEM)) {
            *ppv = static_cast<IKFAsyncWorkItem_I*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }

    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }

public:
    virtual WorkItemState GetItemState() { return _state; }
    virtual bool GetAsyncResult(IKFAsyncResult_I** result)
    {
        if (_result == nullptr || result == nullptr)
            return false;
        *result = _result;
        (*result)->Retain();
        return true;
    }
};

// ***************

class ThreadWorker : public IKFAsyncThreadWorker_I, protected KFThreadObject
{
    KF_IMPL_DECL_REFCOUNT;

    IKFArrayList* _task_queue; //任务队列存储对象
    void* _task_notify_event; //通知有任务到达了，需要执行
    bool _thread_running; //任务线程是否在运行中
    int _timeout_ms; //多久没任务就退出线程（回收资源）
    long long _cur_task_exec_start_time; //本次任务执行的开始时刻
    volatile KREF _task_has_moved; //本次线程的任务队列是不是已经被移动

    KFMutex _mutex;
    char* _name;

public:
    ThreadWorker() = delete;
    explicit ThreadWorker(const char* name) throw() :
        _ref_count(1),
        _task_queue(nullptr),
        _task_notify_event(nullptr),
        _thread_running(false),
        _name(nullptr),
        _timeout_ms(0),
        _cur_task_exec_start_time(-1),
        _task_has_moved(0)
    { if (name) _name = strdup(name); }
    virtual ~ThreadWorker() throw()
    { if (_task_queue) _task_queue->Recycle(); if (_task_notify_event) KFEventDestroy(_task_notify_event); if (_name) free(_name); }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _INTERNAL_KF_INTERFACE_ID_ASYNC_THREAD_WORKER)) {
            *ppv = static_cast<IKFAsyncThreadWorker_I*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }

    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }
    
public:
    virtual KF_RESULT TaskQueueStartup(bool delayRunThread)
    {
        KFLOG_T("%s -> TaskQueueStartup", "ThreadWorker");

        if (_task_queue != nullptr || _task_notify_event != nullptr) {
            KFLOG_ERROR_T("%s -> TaskQueueStartup: Re-entry.", "ThreadWorker");
            return KF_RE_ENTRY;
        }

        //创建任务队列存储的 ArrayList 对象
        if (KF_FAILED(KFCreateObjectArrayList(&_task_queue))) {
            KFLOG_ERROR_T("%s -> TaskQueueStartup: KFCreateBaseObjectList Failed.", "ThreadWorker");
            return KF_INVALID_OBJECT;
        }

        //创建通知执行或者退出线程的 Event 对象
        _task_notify_event = KFEventCreate(0, 1);
        if (_task_notify_event == nullptr) {
            KFLOG_ERROR_T("%s -> TaskQueueStartup: KFEventCreate Failed.", "ThreadWorker");
            return KF_INVALID_STATE;
        }

        //启动任务队列线程
        if (!delayRunThread && !StartThread()) {
            KFLOG_ERROR_T("%s -> TaskQueueStartup: StartThread Failed.", "ThreadWorker");
            return KF_ERROR;
        }
        return KF_OK;
    }
    
    virtual KF_RESULT TaskQueueShutdown()
    {
        KFLOG_T("%s -> TaskQueueShutdown", "ThreadWorker");

        //建立一个 WorkItem 对象，标记为 “请求退出” 模式
        auto req = new(std::nothrow) WorkItem(IKFAsyncWorkItem_I::WorkItemState::RequestExit, nullptr);
        if (req == nullptr)
            return KF_OUT_OF_MEMORY;
        
        {
            KFMutex::AutoLock lock(_mutex);
            _task_queue->InsertElementAt(0, req); //插入到任务队列头，保证立即执行
            KFEventSet(_task_notify_event); //通知事件处理任务队列

            KFLOG_T("%s -> TaskQueueShutdown: Notify event to exit.", "ThreadWorker");
        }
        
        req->Recycle();
        return KF_OK;
    }
    
    virtual KF_RESULT PutItem(IKFAsyncWorkItem_I* asyncItem, WorkItemPriority level)
    {
        KFLOG_T("%s -> PutItem (Level %d)", "ThreadWorker", int(level));

        if (asyncItem == nullptr)
            return KF_INVALID_ARG;
        
        bool result = false;
        KFMutex::AutoLock lock(_mutex);
        int curTaskCount = _task_queue->GetElementCount();
        KFLOG_T("%s -> PutItem (PendingTaskCount:%d)", "ThreadWorker", curTaskCount);

        //优先级算法
        if (level == WorkItemPriority::WorkItemLevel0) {
            result = _task_queue->AddElement(asyncItem);
        }else if (level == WorkItemLevel1) {
            int pos = curTaskCount >> 1;
            if (pos == 0)
                result = _task_queue->AddElement(asyncItem);
            else
                result = _task_queue->InsertElementAt(pos - 1, asyncItem);
        }else if (level == WorkItemLevel2) {
            result = _task_queue->InsertElementAt(0, asyncItem);
        }

        //如果线程没有启动，启动线程
        if (!_thread_running) {
            if (!StartThread())
                return KF_ABORT;
        }

        //通知事件，处理任务队列
        if (curTaskCount == 0)
            KFEventSet(_task_notify_event);

        KFLOG_T("%s -> PutItem: Notify event to execute.", "ThreadWorker");
        return KF_OK;
    }
    
    virtual int GetItemCount()
    {
        if (_task_queue == nullptr)
            return 0;
        
        KFMutex::AutoLock lock(_mutex);
        return _task_queue->GetElementCount();
    }

    virtual void SetTimeout(int timeout_ms) { _timeout_ms = timeout_ms; }

    virtual int GetExecuteElapsedTime()
    {
        auto time = _cur_task_exec_start_time;
        if (time < 0) return -1;
        return int(KFGetTick() - time);
    }

    virtual bool IsTaskQueueMoved() { return _task_has_moved == 1; }
    virtual void MoveCurrentTaskQueue(IKFAsyncThreadWorker_I* other)
    {
        if (other == nullptr) return;
        KFMutex::AutoLock lock(_mutex);
        int count = _task_queue->GetElementCount();
        KFLOG_T("%s -> MoveCurrentTaskQueue: Count %d", "ThreadWorker", count);
        if (count > 0) return;
        if (other->AcceptTaskQueueMove(this, _task_queue)) {
            _task_queue->RemoveAllElements();
            _KF_LOCK_SWAP(&_task_has_moved, 1);
        }
    }
    virtual bool AcceptTaskQueueMove(IKFAsyncThreadWorker_I* other, IKFArrayList* queue)
    {
        if (other == nullptr || queue == nullptr) return false;
        KFMutex::AutoLock lock(_mutex);
        if (GetExecuteElapsedTime() >= other->GetExecuteElapsedTime()) return false;
        int addCount = queue->GetElementCount();
        KFLOG_T("%s -> AcceptTaskQueueMove: Count %d", "ThreadWorker", addCount);
        for (int i = 0; i < addCount; i++) {
            IKFBaseObject* task = nullptr;
            queue->GetElementNoRef(i, &task);
            if (task)
                _task_queue->AddElement(task);
        }
        return true;
    }

protected:
    virtual bool StartThread()
    {
        if (_thread_running)
            return true;

        if (!ThreadStart(nullptr, false, _name))
            return false;

        _thread_running = true;
        return true;
    }

    virtual void OnThreadInvoke(void*)
    {
        KFLOG_T("%s -> OnThreadInvoke Begin...", "ThreadWorker");
        Retain();
        auto task = _task_queue; //任务队列对象
        bool skipSyscall = false; //任务大于0的时候直接进入执行不调用KFEventWait
        
        while (1) {
            KFLOG_INFO_T("%s -> OnThreadInvoke: Event_WAIT...", "ThreadWorker");
            bool waitTimeout = false; //是否等待超时（要退出）
            //等待执行任务事件
            if (!skipSyscall) {
                if (_timeout_ms > 0) {
                    waitTimeout = (KFEventWaitTimed(_task_notify_event, _timeout_ms) == KF_EVENT_TIME_OUT);
                } else {
                    KFEventWait(_task_notify_event);
                }
            }
            skipSyscall = false;
            KFLOG_INFO_T("%s -> OnThreadInvoke: %s ", "ThreadWorker", waitTimeout ? "Event_TIMEOUT." : "Event_ACCEPT!");

            if (waitTimeout) {
                KFMutex::AutoLock lock(_mutex);
                if (task->GetElementCount() == 0) {
                    _thread_running = false;
                    break;
                }else{
                    continue;
                }
            }

            IKFAsyncWorkItem_I* workItem = nullptr;
            {
                //弹出最开头的一个 WorkItem 对象
                IKFBaseObject* object = nullptr;
                KFMutex::AutoLock lock(_mutex);
                if (task->RemoveElement(0, &object)) {
                    KFBaseGetInterface(object, _INTERNAL_KF_INTERFACE_ID_ASYNC_WORK_ITEM, &workItem);
                    object->Recycle();
                }
            }
            
            if (workItem == nullptr) {
                KFLOG_WARN_T("%s -> OnThreadInvoke: WorkItem is Invalid.", "ThreadWorker");
                KFMutex::AutoLock lock(_mutex);
                KFEventReset(_task_notify_event); //失败就重设事件，重新等待
                continue;
            }
            
            auto command = workItem->GetItemState(); //取得 WorkItem 的类型
            KFLOG_T("%s -> OnThreadInvoke: WorkItem type is [%s].", "ThreadWorker",
                      (command == IKFAsyncWorkItem_I::WorkItemState::RequestExit ? "Exit" : "Task"));

            //如果是 “请求退出” 类型
            if (command == IKFAsyncWorkItem_I::WorkItemState::RequestExit) {
                KFLOG_T("%s -> OnThreadInvoke: Request exit.", "ThreadWorker");
                KFMutex::AutoLock lock(_mutex);
                workItem->Recycle();
                task->RemoveAllElements(); //清空还没处理的任务队列
                break; //跳出循环体，退出线程
            }
            
            IKFAsyncResult_I* result = nullptr;
            if (!workItem->GetAsyncResult(&result) || result == nullptr) {
                KFLOG_WARN_T("%s -> OnThreadInvoke: AsyncResult is Empty.", "ThreadWorker");
                workItem->Recycle();
                continue;
            }
            
            IKFAsyncCallback* callback = nullptr;
            result->GetCallback(&callback); //取得 Callback 对象

            KFLOG_T("%s -> OnThreadInvoke: Execute callback...", "ThreadWorker");
            _cur_task_exec_start_time = KFGetTick();
            callback->Execute(result); //执行 Callback！
            _cur_task_exec_start_time = -1;
            KFLOG_T("%s -> OnThreadInvoke: Execute callback finished.", "ThreadWorker");
            
            callback->Recycle();
            result->Recycle();
            
            {
                KFMutex::AutoLock lock(_mutex);
                int pendingTaskCount = task->GetElementCount();
                if (pendingTaskCount == 0) {
                    //如果任务队列已经处理完成了，就重新等待事件，不然则重新循环处理任务
                    KFEventReset(_task_notify_event);
                    KFLOG_INFO_T("%s -> OnThreadInvoke: Event_RESET.", "ThreadWorker");
                    if (_task_has_moved) {
                        //如果任务已经被移走，直接退出线程
                        break;
                    }
                } else {
                    //如果当前的任务队列不为空，则不需要执行等待Event
                    skipSyscall = true;
                    KFLOG_INFO_T("%s -> OnThreadInvoke: Process Next... Pending: %d", "ThreadWorker", pendingTaskCount);
                }
            }

            workItem->Recycle();
        }
        KFLOG_T("%s -> OnThreadInvoke Ended.", "ThreadWorker");
    }

    virtual int OnThreadExit() { Recycle(); return 0; }
};

// ***************

class GroupWorker : public IKFAsyncGroupWorker_I
{
    KF_IMPL_DECL_REFCOUNT;
    
    int _max_threads;
    IKFArrayList* _threads;
    
    KFMutex _mutex;
    bool _shutdown;
    char* _name;

public:
    GroupWorker() throw() : _ref_count(1), _max_threads(0), _threads(nullptr), _shutdown(true), _name(nullptr) {}
    virtual ~GroupWorker() throw()
    { if (_threads) _threads->Recycle(); if (_name) free(_name); }
    
public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _INTERNAL_KF_INTERFACE_ID_ASYNC_GROUP_WORKER)) {
            *ppv = static_cast<IKFAsyncGroupWorker_I*>(this);
            Retain();
            return KF_OK;
        }
        return KF_NO_INTERFACE;
    }
    
    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }
    
public:
    virtual KF_RESULT Startup(int max_threads, const char* name)
    {
        KFLOG_T("%s -> Startup", "GroupWorker");

        KFMutex::AutoLock lock(_mutex);
        if (name)
            _name = strdup(name);

        if (_threads == nullptr) {
            //创建线程集合存储对象，存储所有的 ThreadWorker 对象
            if (KF_FAILED(KFCreateObjectArrayList(&_threads))) {
                KFLOG_ERROR_T("%s -> Startup: KFCreateBaseObjectList Failed.", "GroupWorker");
                return KF_INVALID_OBJECT;
            }
        }
        
        if (_threads->GetElementCount() > 0) {
            KFLOG_ERROR_T("%s -> Startup: GetElementCount(Thread) > 0 is Invalid.", "GroupWorker");
            return KF_INVALID_STATE;
        }
        
        //尝试取得系统的 CPU 核心数，用于自动决定最大线程
        _max_threads = max_threads;
        if (_max_threads != KF_ASYNC_WORKER_THREADS_INFINITE) {
            if (_max_threads <= 0)
                _max_threads = KFSystemCpuCount() + 1;
            if (_max_threads == 0)
                _max_threads = 1;
        }

        KFLOG_T("%s -> Startup: Max Threads is %d.", "GroupWorker", _max_threads);
        
        auto thread = CreateThreadWorker(MakeThreadWorkerName(0)); //创建第一个 ThreadWorker 对象
        if (thread == nullptr) {
            KFLOG_ERROR_T("%s -> Startup: CreateThreadWorker Failed.", "GroupWorker");
            return KF_INVALID_OBJECT;
        }
        
        //启动这个 ThreadWorker 的实例线程体
        thread->SetTimeout(WORKER_EXIT_TIMEOUT);
        if (KF_FAILED(thread->TaskQueueStartup(true)) || !_threads->AddElement(thread)) {
            KFLOG_ERROR_T("%s -> Startup: TaskQueueStartup(Thread) Failed.", "GroupWorker");
            thread->Recycle();
            return KF_ERROR;
        }
        thread->Recycle();

        _shutdown = false;
        return KF_OK;
    }
    
    virtual KF_RESULT Shutdown()
    {
        KFLOG_T("%s -> Shutdown", "GroupWorker");

        KFMutex::AutoLock lock(_mutex);
        int count = _threads->GetElementCount();
        KFLOG_T("%s -> Shutdown: Current Threads is %d.", "GroupWorker", count);

        //遍历每个 ThreadWorker 对象
        for (int i = 0; i < count; i++) {
            IKFAsyncThreadWorker_I* thread = nullptr;
            KFGetArrayListElement<IKFAsyncThreadWorker_I>(_threads, i,
                                                           _INTERNAL_KF_INTERFACE_ID_ASYNC_THREAD_WORKER,
                                                           &thread);
            if (thread) {
                KFLOG_T("%s -> Shutdown: Thread %d TaskQueueShutdown.", "GroupWorker", i);
                thread->TaskQueueShutdown(); //关闭每个 ThreadWorker 的任务队列
                thread->Recycle();
            }
        }
        //移除所有 ThreadWorker 对象
        _threads->RemoveAllElements();

        _shutdown = true;
        return KF_OK;
    }
    
    virtual KF_RESULT PutWorkItem(IKFAsyncResult* result, bool realtime)
    {
        KFLOG_T("%s -> PutWorkItem (Real-time: %s)", "GroupWorker", realtime ? "true" : "false");

        if (result == nullptr)
            return KF_INVALID_ARG;
        
        KFMutex::AutoLock lock(_mutex);
        if (_shutdown)
            return KF_SHUTDOWN;

        IKFAsyncResult_I* callback = nullptr;
        KFBaseGetInterface(result, _INTERNAL_KF_INTERFACE_ID_ASYNC_RESULT, &callback);
        if (callback == nullptr) {
            KFLOG_ERROR_T("%s -> PutWorkItem: Callback is Invalid.", "GroupWorker");
            return KF_NO_INTERFACE;
        }
        
        //新建立一个 WorkItem 对象，标记为 “执行任务” 模式
        auto workItem = new(std::nothrow) WorkItem(IKFAsyncWorkItem_I::WorkItemState::ExecuteTask, callback);
        if (workItem == nullptr) {
            KFLOG_ERROR_T("%s -> PutWorkItem: Alloc Memory Failed.", "GroupWorker");
            callback->Recycle();
            return KF_OUT_OF_MEMORY;
        }
        callback->Recycle();

        //提交这个 WorkItem 去某个 ThreadWorker 执行
        auto r = InternalPutWorkItem(workItem,
                                     realtime ?
                                     IKFAsyncThreadWorker_I::WorkItemPriority::WorkItemLevel2 :
                                     IKFAsyncThreadWorker_I::WorkItemPriority::WorkItemLevel0);
        if (KF_FAILED(r)) {
            KFLOG_ERROR_T("%s -> PutWorkItem: InternalPutWorkItem Failed.", "GroupWorker");
            workItem->Recycle();
            return r;
        }
        
        workItem->Recycle();
        return KF_OK;
    }
    
    virtual int GetCurrentThreads()
    {
        KFMutex::AutoLock lock(_mutex);
        return _threads->GetElementCount();
    }
    
private:
    char* MakeThreadWorkerName(int index)
    {
        if (_name == nullptr)
            return nullptr;

        auto c = (char*)malloc(strlen(_name) + 128);
        if (c)
            sprintf(c, "%s : Worker %d", _name, index);
        return c;
    }

    ThreadWorker* CreateThreadWorker(char* name)
    {
        auto t = new(std::nothrow) ThreadWorker(name);
        if (name)
            free(name); //MakeThreadWorkerName
        return t;
    }
    
    KF_RESULT InternalPutWorkItem(IKFAsyncWorkItem_I* workItem, IKFAsyncThreadWorker_I::WorkItemPriority level)
    {
        KFMutex::AutoLock lock(_mutex);
        KF_RESULT result = KF_OK;
        int count = _threads->GetElementCount();
        KFLOG_T("%s -> InternalPutWorkItem (Thread Count %d)", "GroupWorker", count);

        bool toCreateNew = (count < _max_threads);
        if (_max_threads == KF_ASYNC_WORKER_THREADS_INFINITE)
            toCreateNew = true; //无上限创建新的线程数

        //如果当前已经创建的 ThreadWorker 小于可创建的最大数目，则可以创建新的执行 Callback
        if (toCreateNew) {
            KFLOG_T("%s -> InternalPutWorkItem: Select to CreateThread.", "GroupWorker");
            IKFAsyncThreadWorker_I* thread = nullptr;
            for (int i = 0; i < count; i++) {
                //优先找已经创建的线程中的空闲中的无任务的线程来执行
                if (thread) {
                    thread->Recycle();
                    thread = nullptr;
                }
                KFGetArrayListElement<IKFAsyncThreadWorker_I>(_threads, i,
                                                              _INTERNAL_KF_INTERFACE_ID_ASYNC_THREAD_WORKER,
                                                              &thread);
                if (thread) {
                    if (thread->GetItemCount() == 0) //有无任务的线程
                        break; //使用这个线程
                }
            }
            
            if (thread->GetItemCount() == 0) {
                //如果有线程处于无任务状态，不需要创建新的了，直接提交给这个线程执行
                result = thread->PutItem(workItem, level);
            }else{
                //创建新的线程执行任务
                thread->Recycle();
                thread = CreateThreadWorker(MakeThreadWorkerName(count)); //创建一个新的 ThreadWorker
                if (thread == nullptr) {
                    KFLOG_ERROR_T("%s -> InternalPutWorkItem: CreateThreadWorker Failed.", "GroupWorker");
                    return KF_OUT_OF_MEMORY;
                }

                //启动这个线程的任务队列
                thread->SetTimeout(WORKER_EXIT_TIMEOUT);
                if (KF_FAILED(thread->TaskQueueStartup(false))) {
                    KFLOG_ERROR_T("%s -> InternalPutWorkItem: TaskQueueStartup Failed.", "GroupWorker");
                    thread->Recycle();
                    return KF_INVALID_STATE;
                }

                //提交任务
                if (KF_FAILED(thread->PutItem(workItem, level))) {
                    thread->TaskQueueShutdown();
                    thread->Recycle();
                    return KF_INVALID_STATE;
                }

                //把线程添加到组
                result = _threads->AddElement(thread) ? KF_OK : KF_OUT_OF_MEMORY;
            }
            thread->Recycle();
        }else{
            KFLOG_T("%s -> InternalPutWorkItem: Select to ReserveThread.", "GroupWorker");
            //如果线程已经饱和，则要提交到一个当前任务最少的线程
            if (count == 1) {
                //如果只允许创建一个 ThreadWorker
                IKFAsyncThreadWorker_I* thread = nullptr;
                KFGetArrayListElement<IKFAsyncThreadWorker_I>(_threads, 0,
                                                               _INTERNAL_KF_INTERFACE_ID_ASYNC_THREAD_WORKER,
                                                               &thread);
                if (thread) {
                    result = thread->PutItem(workItem, level); //提交执行
                    thread->Recycle();
                }
            }else{
                //选一个任务最少的线程
                IKFAsyncThreadWorker_I* thread = nullptr;
                int exec_thread_index = -1;
                
                int cur_thread_index = 0;
                int cur_thread_tasks = INT32_MAX;
                for (int i = 0; i < count; i++) {
                    KFGetArrayListElement<IKFAsyncThreadWorker_I>(_threads, i,
                                                                   _INTERNAL_KF_INTERFACE_ID_ASYNC_THREAD_WORKER,
                                                                   &thread);
                    if (thread) {
                        int task_count = thread->GetItemCount();
                        if (task_count == 0) {
                            exec_thread_index = i;
                            thread->Recycle();
                            break;
                        }
                        if (task_count < cur_thread_tasks) {
                            cur_thread_tasks = task_count;
                            cur_thread_index = i;
                        }
                        thread->Recycle();
                    }
                    exec_thread_index = cur_thread_index;
                }
                
                KFGetArrayListElement<IKFAsyncThreadWorker_I>(_threads, exec_thread_index,
                                                               _INTERNAL_KF_INTERFACE_ID_ASYNC_THREAD_WORKER,
                                                               &thread);
                result = thread->PutItem(workItem, level); //提交执行
                thread->Recycle();

                KFLOG_T("%s -> InternalPutWorkItem: Submit to Thread %d", "GroupWorker", exec_thread_index);
            }
        }
        return result;
    }
};

// ***************

static KFMutex kGlobalDefaultQueue_Mutex;
static KREF kGlobalDefaultQueue_RefCount = 0;
static KASYNCOBJECT kGlobalDefaultQueue_Serial = nullptr;
static KASYNCOBJECT kGlobalDefaultQueue_Parallel = nullptr;

static bool KFGlobalStartup()
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (kGlobalDefaultQueue_Serial != nullptr ||
        kGlobalDefaultQueue_Parallel != nullptr)
        return false;
    if (KF_FAILED(KFAsyncCreateWorker(false, 1, &kGlobalDefaultQueue_Serial)))
        return false;
    if (KF_FAILED(KFAsyncCreateWorker(true, 0, &kGlobalDefaultQueue_Parallel)))
        return false;
    return true;
#else
    return KFAsyncMFStartup();
#endif
}

static void KFGlobalShutdown()
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (kGlobalDefaultQueue_Serial)
        KFAsyncDestroyWorker(kGlobalDefaultQueue_Serial);
    if (kGlobalDefaultQueue_Parallel)
        KFAsyncDestroyWorker(kGlobalDefaultQueue_Parallel);
    kGlobalDefaultQueue_Serial = kGlobalDefaultQueue_Parallel = nullptr;
#else
    KFAsyncMFShutdown();
#endif
}

// ***************

bool KFAPI KFAsyncStartup()
{
    //创建默认的全局任务队列，供 KFAsyncInvokeCallback 执行
    if (kGlobalDefaultQueue_Mutex.Get() == nullptr) //检测全局锁是否初始化OK
        return false;

    KFMutex::AutoLock lock(&kGlobalDefaultQueue_Mutex);
    if (kGlobalDefaultQueue_RefCount > 0)
        return true;
    kGlobalDefaultQueue_RefCount = 1;
    return KFGlobalStartup();
}

void KFAPI KFAsyncShutdown()
{
    KFMutex::AutoLock lock(&kGlobalDefaultQueue_Mutex);
    kGlobalDefaultQueue_RefCount -= 1;
    if (kGlobalDefaultQueue_RefCount == 0)
        KFGlobalShutdown();
}

bool KFAPI KFAsyncLockRef()
{
    KFMutex::AutoLock lock(&kGlobalDefaultQueue_Mutex);
    if (kGlobalDefaultQueue_RefCount == 0)
        return KFAsyncStartup();
    kGlobalDefaultQueue_RefCount += 1;
    return true;
}

void KFAPI KFAsyncUnlockRef()
{
    KFAsyncShutdown();
}

// ***************

#ifndef KF_WIN_MF_WORKQUEUE
struct AsyncWorkerObject
{
    KREF RefCount;
    GroupWorker* Worker;
    char* Name;
    int ThreadCount;
};
#endif

KF_RESULT KFAPI KFAsyncCreateWorker(bool parallel, int max_threads, KASYNCOBJECT* asyncObject, const char* worker_name)
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (asyncObject == nullptr)
        return KF_INVALID_PTR;

    auto object = (AsyncWorkerObject*)malloc(sizeof(AsyncWorkerObject));
    if (object == nullptr)
        return KF_OUT_OF_MEMORY;
    
    //新建一个线程组对象
    auto worker = new(std::nothrow) GroupWorker();
    if (worker == nullptr) {
        free(object);
        return KF_OUT_OF_MEMORY;
    }
    
    //启动第一个线程体
    auto name = worker_name;
    if (name)
        if (strlen(name) == 1)
            name = nullptr;

    if (KF_FAILED(worker->Startup(parallel ? max_threads : 1, name))) {
        worker->Recycle();
        free(object);
        return KF_INIT_ERROR;
    }

    object->RefCount = 1;
    object->Worker = worker;
    object->ThreadCount = max_threads;
    object->Name = nullptr;
    if (worker_name)
        object->Name = strdup(worker_name);
    
    *asyncObject = object;
    return KF_OK;
#else
    return KFAsyncCreateWorker_Win32_MF(parallel, max_threads, asyncObject, worker_name);
#endif
}

KF_RESULT KFAPI KFAsyncDestroyWorker(KASYNCOBJECT worker)
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (worker == nullptr)
        return KF_INVALID_ARG;
    
    auto my = reinterpret_cast<AsyncWorkerObject*>(worker);
    KREF rc = _KFRefDec(&my->RefCount);
    if (rc == 0) {
        my->Worker->Shutdown(); //退出所有线程（异步）
        my->Worker->Recycle();

        if (my->Name)
            free(my->Name);
        free(my);
    }
    return KF_OK;
#else
    return KFAsyncDestroyWorker_Win32_MF(worker);
#endif
}

KF_RESULT KFAPI KFAsyncWorkerLockRef(KASYNCOBJECT worker)
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (worker == nullptr)
        return KF_INVALID_ARG;

    auto my = reinterpret_cast<AsyncWorkerObject*>(worker);
    _KFRefInc(&my->RefCount);
    return KF_OK;
#else
    return KFAsyncWorkerLockRef_Win32_MF(worker);
#endif
}

KF_RESULT KFAPI KFAsyncWorkerUnlockRef(KASYNCOBJECT worker)
{
#ifndef KF_WIN_MF_WORKQUEUE
    return KFAsyncDestroyWorker(worker);
#else
    return KFAsyncDestroyWorker_Win32_MF(worker);
#endif
}

KF_RESULT KFAPI KFAsyncGetWorkerMaxThreads(KASYNCOBJECT worker, int* threads)
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (worker == nullptr)
        return KF_INVALID_ARG;
    if (threads == nullptr)
        return KF_INVALID_PTR;

    auto my = reinterpret_cast<AsyncWorkerObject*>(worker);
    *threads = my->ThreadCount;
    return KF_OK;
#else
    return KF_NOT_SUPPORTED;
#endif
}

KF_RESULT KFAPI KFAsyncGetWorkerName(KASYNCOBJECT worker, char* worker_name, int* name_len)
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (worker == nullptr)
        return KF_INVALID_ARG;

    auto my = reinterpret_cast<AsyncWorkerObject*>(worker);
    if (my->Name == nullptr)
        return KF_NOT_FOUND;

    int len = (int)strlen(my->Name);
    if (worker_name == nullptr) {
        if (name_len != nullptr)
            *name_len = len + 1;
        else
            return KF_UNEXPECTED;
    }else{
        if (name_len == nullptr) {
            strcpy(worker_name, my->Name);
        }else{
            if (*name_len > len)
                strcpy(worker_name, my->Name);
            else
                memcpy(worker_name, my->Name, *name_len - 1);
        }
    }
    return KF_OK;
#else
    return KF_NOT_SUPPORTED;
#endif
}

KF_RESULT KFAPI KFAsyncPutWorkItemEx(KASYNCOBJECT worker, IKFAsyncResult* result)
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (worker == nullptr || result == nullptr)
        return KF_INVALID_ARG;
    
    auto selector = worker;
    if (worker == KF_ASYNC_GLOBAL_WORKER_SINGLE_THREAD)
        selector = kGlobalDefaultQueue_Serial;
    else if (worker == KF_ASYNC_GLOBAL_WORKER_MULTI_THREAD)
        selector = kGlobalDefaultQueue_Parallel;

    auto my = reinterpret_cast<AsyncWorkerObject*>(selector);
    return my->Worker->PutWorkItem(result, false);
#else
    return KFAsyncPutWorkItemEx_Win32_MF(worker, result);
#endif
}

KF_RESULT KFAPI KFAsyncPutWorkItem(KASYNCOBJECT worker, IKFAsyncCallback* callback, IKFBaseObject* state)
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (worker == nullptr || callback == nullptr)
        return KF_INVALID_ARG;

    IKFAsyncResult* result = nullptr;
    auto r = KFAsyncCreateResult(callback, state, nullptr, &result);
    _KF_FAILED_RET(r);

    r = KFAsyncPutWorkItemEx(worker, result);
    result->Recycle();
    return r;
#else
    return KFAsyncPutWorkItem_Win32_MF(worker, callback, state);
#endif
}

KF_RESULT KFAPI KFAsyncInvokeCallback(IKFAsyncResult* result)
{
#ifndef KF_WIN_MF_WORKQUEUE
    if (kGlobalDefaultQueue_Parallel == nullptr ||
        result == nullptr)
        return KF_INVALID_ARG;
    return KFAsyncPutWorkItemEx(kGlobalDefaultQueue_Parallel, result);
#else
    return KFAsyncInvokeCallback_Win32_MF(result);
#endif
}