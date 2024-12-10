#include <utils/auto_mutex.hxx>
#include <base/kf_log.hxx>
#include <base/kf_ptr.hxx>
#include <async/kf_thread_object.hxx>
#include <async/kf_timed_event_internal.hxx>

#define KF_LOG_TAG_STR "kf_timed_event.cxx"

#ifdef _MSC_VER
#pragma warning(disable:4127)
#endif

class TimedEventQueue : public IKFTimedEventQueue_I, protected KFThreadObject
{
    KF_IMPL_DECL_REFCOUNT;

    bool _bStarted, _bStopped; //状态指示符
    KFPtr<IKFFastList> _queue; //任务队列
    KFMutex _mutex;

    void* _cvQueueNotEmpty; //通知任务队列不为空
    void* _cvQueueHeadChanged; //通知任务队列第一个元素改变了

    KF_TIMED_EVENT_ID _nextEventId; //记录事件ID

public:
    TimedEventQueue() throw() :
    _ref_count(1), _mutex(true),
    _nextEventId(TIMED_QUEUE_STARTUP_EVENT_ID),
    _bStarted(false), _bStopped(false) {}
    virtual ~TimedEventQueue() throw()
    { Shutdown(SkipTasks, SyncShutdown); DestroyCondVars(); }

public:
    virtual KF_RESULT CastToInterface(KIID interface_id, void** ppv)
    {
        KF_IMPL_CHECK_PARAM;
        if (_KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_BASE_OBJECT) ||
            _KFInterfaceIdEqual(interface_id, _KF_INTERFACE_ID_TIMED_EVENT_QUEUE) ||
            _KFInterfaceIdEqual(interface_id, _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_QUEUE)) {
            *ppv = static_cast<IKFTimedEventQueue_I*>(this);
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
    virtual KF_RESULT Startup()
    {
        KFLOG_T("%s -> Startup.", "TimedEventQueue")
        if (_bStarted)
            return KF_RE_ENTRY;

        _nextEventId = TIMED_QUEUE_STARTUP_EVENT_ID;
        auto r = KFCreateObjectFastList(&_queue); //创建任务队列
        if (KF_FAILED(r)) {
            KFLOG_ERROR_T("%s -> Startup: KFCreateObjectFastList Failed.", "TimedEventQueue")
            return r;
        }

        //创建条件变量
        if (!CreateCondVars()) {
            KFLOG_ERROR_T("%s -> Startup: KFCondVarCreate Failed.", "TimedEventQueue")
            return KF_INVALID_STATE;
        }

        //启动任务队列线程
        if (!ThreadStart()) {
            KFLOG_ERROR_T("%s -> ThreadStart Failed.", "TimedEventQueue");
            return KF_ERROR;
        }

        _bStarted = true;
        _bStopped = false;
        return KF_OK;
    }

    virtual KF_RESULT Shutdown(QueueShutdownFlushState flush_state, QueueShutdownAsyncMode async_mode)
    {
        KFLOG_T("%s -> Shutdown.", "TimedEventQueue");
        if (_bStopped)
            return KF_RE_ENTRY;
        
        KFPtr<IKFTimedEventState> s;
        auto r = KFCreateTimedEventState(nullptr, nullptr, &s); //创建一个通知退出事件
        if (KF_FAILED(r)) {
            KFLOG_ERROR_T("%s -> KFCreateTimedEventState Failed.", "TimedEventQueue");
            return r;
        }
        
        KFPtr<IKFTimedEventState_I> state;
        KFBaseGetInterface(s.Get(), _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE, &state);

        if (async_mode == AsyncShutdown) //异步退出
            ThreadDetach(); //抛弃工作线程
        
        //设置这个事件为请求队列线程中止
        state->SetEventType(IKFTimedEventState_I::EventStateType::QueueAbort);
        if (flush_state == ExecuteTasks) //如果flush为Execute，插入到最后，等待前面的所有事件执行完成，此会方式卡死调用者
            r = PostEventToBack(s.Get(), nullptr);
        else
            r = PostTimedEvent(s.Get(), INT64_MIN, nullptr, true) ? KF_OK : KF_ERROR; //插入到最前
        
        KFLOG_T("%s -> PostEvent to Notify Exit. (Flush: %s)", "TimedEventQueue", (flush_state == ExecuteTasks ? "True" : "False"));
        if (KF_FAILED(r)) {
            KFLOG_ERROR_T("%s -> PostEvent Failed.", "TimedEventQueue");
            return r;
        }
        
        if (async_mode == SyncShutdown) { //同步退出
            KFLOG_T("%s -> ThreadJoin Begin...", "TimedEventQueue");
            ThreadJoin(); //等待任务队列线程退出
            KFLOG_T("%s -> ThreadJoin Ended.", "TimedEventQueue");

            //清空任务队列
            if (_queue) {
                _queue->Clear();
                _queue.Reset();
            }
            //销毁条件变量
            DestroyCondVars();
        }
        
        _bStopped = true;
        _bStarted = false;
        return KF_OK;
    }

    virtual KF_RESULT PostEvent(IKFTimedEventState* callback, KF_TIMED_EVENT_ID* id)
    { return PostTimedEvent(callback, INT64_MIN + 1, id, false) ? KF_OK : KF_ERROR; }
    virtual KF_RESULT PostEventToBack(IKFTimedEventState* callback, KF_TIMED_EVENT_ID* id)
    { return PostTimedEvent(callback, INT64_MAX, id, false) ? KF_OK : KF_ERROR; }
    virtual KF_RESULT PostEventWithDelay(IKFTimedEventState* callback, KF_INT32 delay_ms, KF_TIMED_EVENT_ID* id)
    { return PostTimedEvent(callback, KFGetTick() + (KF_INT64)delay_ms, id, true) ? KF_OK : KF_ERROR; }

    virtual KF_RESULT CancelEvent(KF_TIMED_EVENT_ID id)
    {
        if (id == TIMED_QUEUE_INVALID_EVENT_ID)
            return KF_INVALID_ARG;
        
        KFMutex::AutoLock lock(_mutex);
        KFLOG_INFO_T("%s -> CancelEvent %d (Queue-count: %d)", "TimedEventQueue", id, _queue->GetCount());
        auto node = _queue->Begin();
        while (node != _queue->End()) { //循环遍历所有的任务队列元素
            KFPtr<IKFTimedEventState_I> state;
            KFGetFastListObject<IKFTimedEventState_I>(_queue, node, _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE, &state);
            
            if (state->GetEventId() == id) { //如果找到这个任务项目
                KFLOG_T("%s -> CancelEvent Found.", "TimedEventQueue");
                if (node == _queue->Begin()) //如果要移除的项目是任务头
                    KFCondVarSignal(_cvQueueHeadChanged); //通知条件变量，任务头改变，当前任务被移除，执行下一个任务
                
                state->SetEventId(TIMED_QUEUE_INVALID_EVENT_ID); //设置为无效的事件
                _queue->Remove(node, nullptr); //从任务队列中移除
                return KF_OK;
            }
            node = _queue->Next(node);
        }
        return KF_NOT_FOUND;
    }
    
    virtual KF_RESULT CancelAllEvents()
    {
        KFLOG_T("%s -> CancelAllEvents.", "TimedEventQueue");
        KFMutex::AutoLock lock(_mutex);
        KFCondVarSignal(_cvQueueHeadChanged); //通知正在等待就立即退出
        _queue->Clear(); //移除所有的还未执行的事件
        return KF_OK;
    }

    virtual int GetPendingEventCount()
    {
        KFMutex::AutoLock lock(_mutex);
        return _queue->GetCount();
    }

public:
    virtual void* GetThread()
    { return ThreadObject(); }
    virtual IKFFastList* GetQueue()
    { return _queue; }

    virtual bool IsStartup()
    { return _bStarted; }
    virtual bool IsStopped()
    { return _bStopped; }

private:
    bool CreateCondVars()
    {
        _cvQueueNotEmpty = KFCondVarCreate(); //队列不为空事件
        _cvQueueHeadChanged = KFCondVarCreate(); //队列头更新事件（通知最优先执行的任务已经改变）
        return (_cvQueueNotEmpty != nullptr && _cvQueueHeadChanged != nullptr);
    }
    void DestroyCondVars()
    {
        if (_cvQueueNotEmpty)
            KFCondVarDestroy(_cvQueueNotEmpty);
        if (_cvQueueHeadChanged)
            KFCondVarDestroy(_cvQueueHeadChanged);
        _cvQueueNotEmpty = _cvQueueHeadChanged = nullptr;
    }

    bool PostTimedEvent(IKFTimedEventState* callback, KF_INT64 realtime, KF_TIMED_EVENT_ID* id, bool allow_prev)
    {
        if (!_bStarted)
            return false;
        if (callback == nullptr)
            return false;

        KFMutex::AutoLock lock(_mutex);
        KFPtr<IKFTimedEventState_I> state;
        if (KF_FAILED(KFBaseGetInterface(callback, _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE, &state)))
            return false;

        KFLOG_T("%s -> PostTimedEvent (Realtime: %lld, Id: %d)...", "TimedEventQueue", realtime, _nextEventId);
        if (id)
            *id = _nextEventId; //本次任务的ID
        state->SetEventId(_nextEventId++);
        state->SetTime(realtime); //本次任务的期望执行时间（现实世界时间）

        int index = 0;
        bool time_is_equal = false;
        auto node = _queue->Begin();
        while (node != _queue->End()) {
            KFPtr<IKFTimedEventState_I> task;
            KFGetFastListObject<IKFTimedEventState_I>(_queue, node, _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE, &task);

            KFLOG_T("%s -> PostTimedEvent %d ItemTime: %lld", "TimedEventQueue", index, state->GetTime());
            if (task->GetTime() >= realtime) {
                //找到一个比当前任务期望执行时间更大的任务
                KFLOG_T("%s -> PostTimedEvent InsertIn: %d", "TimedEventQueue", index);
                time_is_equal = (task->GetTime() == realtime);
                break;
            }
            
            node = _queue->Next(node);
            index++;
        }
        
        auto saved_node = node;
        if (node != nullptr && allow_prev && !time_is_equal) {
            node = _queue->Prev(node); //取这个任务的上一个任务，保证按照时间从近到远的线性执行顺序
            KFLOG_T("%s -> PostTimedEvent: Do Prev.", "TimedEventQueue");
        }
        if (saved_node == _queue->Begin()) { //如果被改变的任务在任务队列头
            KFLOG_T("%s -> PostTimedEvent: Wake QueueHeadChanged.", "TimedEventQueue");
            KFCondVarSignal(_cvQueueHeadChanged); //通知任务队列头改变，如果正在等待更久远的任务，立即退出，执行新的最优先的任务
        }
        
        if (saved_node == _queue->Begin())
            _queue->PushFront(callback); //插入到任务队列头
        else
            _queue->InsertBack(node, callback); //插入到任务队列任意合适的位置
        
        KFLOG_T("%s -> PostTimedEvent: Wake QueueNotEmpty. (QueueSize: %d)", "TimedEventQueue", _queue->GetCount());
        KFCondVarSignal(_cvQueueNotEmpty); //通知任务队列有项目了，该去执行了
        return true;
    }

    bool CheckAndRemoveEventById(KF_TIMED_EVENT_ID id) //根据ID检查项目是不是在任务队列中
    {
        auto node = _queue->Begin();
        while (node != _queue->End()) {
            KFPtr<IKFTimedEventState_I> state;
            KFGetFastListObject<IKFTimedEventState_I>(_queue, node, _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE, &state);

            if (state->GetEventId() == id) {
                _queue->Remove(node, nullptr);
                return true;
            }

            node = _queue->Next(node);
        }
        return false;
    }

protected:
    virtual void OnThreadInvoke(void*)
    {
        KFLOG_T("%s -> OnThreadInvoke Begin...", "TimedEventQueue");
        Retain();
        while (1)
        {
            KFPtr<IKFTimedEventCallback> callback;
            KFPtr<IKFTimedEventState_I> state;
            {
                KFMutex::AutoLock lock(_mutex);
                if (_bStopped) {
                    //异步销毁条件变量
                    DestroyCondVars();
                    //异步销毁任务队列
                    _queue->Clear();
                    _queue.Reset();
                    break;
                }
                
                while (_queue->IsEmpty()) {
                    KFLOG_T("%s -> Enter: QueueNotEmpty...", "ThreadInvoke");
                    KFCondVarWait(_cvQueueNotEmpty, _mutex.Get()); //等待任务队列不为空事件
                }
                KFLOG_T("%s -> Leave: QueueNotEmpty.", "ThreadInvoke");
                
                KF_TIMED_EVENT_ID eventId = TIMED_QUEUE_INVALID_EVENT_ID;
                while (1) {
                    if (_queue->IsEmpty()) {
                        //在KFCondVarWaitTimed等待期间，任务队列可能已经被清空
                        KFLOG_WARN_T("%s -> Loop Break: Queue is Empty.", "ThreadInvoke");
                        break;
                    }
                    
                    KFPtr<IKFBaseObject> event;
                    if (!_queue->GetFront(&event)) {
                        KFLOG_WARN_T("%s -> GetFront is None.", "ThreadInvoke");
                        break;
                    }
                    
                    KFBaseGetInterface(event.Get(), _INTERNAL_KF_INTERFACE_ID_TIMED_EVENT_STATE, &state);
                    if (state == nullptr) {
                        KFLOG_WARN_T("%s -> Invalid Event Object.", "ThreadInvoke");
                        break;
                    }
                    
                    eventId = state->GetEventId();
                    KFLOG_INFO_T("%s -> GetNextEvent %d", "ThreadInvoke", eventId);
                    
                    KF_INT64 currentTime = KFGetTick(); //当前的时间
                    KF_INT64 requestTime = state->GetTime(); //期望要执行的时间（语义上是“未来的时间”）
                    
                    int sleepTime;
                    if (requestTime < 0 || requestTime == INT64_MAX) //特殊处理，比如 QueueAbort 的情况
                        sleepTime = 0;
                    else
                        sleepTime = (int)(requestTime - currentTime); //计算未来的时间相对当前时间的offset
                
                    KFLOG_T("%s -> SleepTime: %d", "ThreadInvoke", sleepTime);
                    if (sleepTime <= 0) //如果期望的时间已经到了，甚至过了（负数）
                        break; //现在要立即执行，退出循环
                
                    bool timeoutCapped = false;
                    if (sleepTime > 10000) { //最大只做一次10秒到等待，超过10秒到任务会分批等待
                        sleepTime = 10000;
                        timeoutCapped = true;
                        KFLOG_T("%s -> Max Timeout is 10 sec.", "ThreadInvoke");
                    }
                    
                    KFLOG_T("%s -> TimedWait Enter. %lld", "ThreadInvoke", KFGetTick());
                    //等待当前的任务到达指定的时间，这里会解锁互斥锁，所以外部可以Post新的任务进来
                    //如果新的任务的执行时间比现在要等待到的期望时间更早，则QueueHeadChanged会被通知
                    //函数退出等待，然后执行时间更早的更优先的任务，当前等待的任务被延后执行。
                    auto r = KFCondVarWaitTimed(_cvQueueHeadChanged, _mutex.Get(), sleepTime);
                    KFLOG_T("%s -> TimedWait Leave. %lld", "ThreadInvoke", KFGetTick());
                    if (!timeoutCapped && r == KF_EVENT_TIME_OUT) //判断是不是10超时分批等待（timeoutCapped）
                        break; //如果不是，那时间到了，去执行任务
                }
                
                if (eventId != TIMED_QUEUE_INVALID_EVENT_ID && CheckAndRemoveEventById(eventId))
                    state->GetCallback(callback.ResetAndGetAddressOf()); //从队列中移除任务并取得callback
            }
            
            if (callback != nullptr) {
                if (state->GetEventType() == IKFTimedEventState_I::EventStateType::MethodInvoke) {
                    KFLOG_T("%s -> Execute Event: %d", "ThreadInvoke", state->GetEventId());
                    callback->Invoke(state.Get(), this); //执行callback
                }
            }else if (state != nullptr) {
                if (state->GetEventType() == IKFTimedEventState_I::EventStateType::QueueAbort) {
                    _bStopped = true; //如果是通过Shutdown调用发出来的QueueAbort任务，就退出整个线程的循环
                    KFLOG_INFO_T("%s -> Request Queue Abort.", "ThreadInvoke");
                }
            }
        }
        KFLOG_T("%s -> OnThreadInvoke Ended.", "TimedEventQueue");
    }

    virtual int OnThreadExit() { Recycle(); return 0; }
};

// ***************

KF_RESULT KFAPI KFCreateTimedEventQueue(IKFTimedEventQueue** queue)
{
    if (queue == nullptr)
        return KF_INVALID_PTR;

    auto q = new(std::nothrow) TimedEventQueue();
    if (q == nullptr)
        return KF_OUT_OF_MEMORY;

    *queue = q;
    return KF_OK;
}