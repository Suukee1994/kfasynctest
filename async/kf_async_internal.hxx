#ifndef __KF_ASYNC__ASYNC_INTERNAL_H
#define __KF_ASYNC__ASYNC_INTERNAL_H

#include <async/kf_async_abstract.hxx>
#include <base/kf_array_list.hxx>

#ifndef KF_INTERFACE_ID_USE_GUID
#define _INTERNAL_KF_INTERFACE_ID_ASYNC_RESULT "__kf_iid_async_result"
#else
#define _INTERNAL_KF_INTERFACE_ID_ASYNC_RESULT "_9DB9CAEA7B3B4CFCAB58C63FFE6AC86F"
#endif
struct IKFAsyncResult_I : public IKFAsyncResult
{
    virtual void SetCallback(IKFAsyncCallback* callback) = 0;
    virtual void GetCallback(IKFAsyncCallback** callback) = 0;
};

#ifndef KF_INTERFACE_ID_USE_GUID
#define _INTERNAL_KF_INTERFACE_ID_ASYNC_WORK_ITEM "__kf_iid_async_work_item"
#else
#define _INTERNAL_KF_INTERFACE_ID_ASYNC_WORK_ITEM "_2AC68D7B300342CE9AFD5F97E5E8BE41"
#endif
struct IKFAsyncWorkItem_I : public IKFBaseObject
{
    enum WorkItemState
    {
        ExecuteTask, //执行 IKFAsyncResult 中的 IKFAsyncCallback->Invoke
        RequestExit  //退出 当前的 ThreadWorker
    };
    virtual WorkItemState GetItemState() = 0;
    virtual bool GetAsyncResult(IKFAsyncResult_I** result) = 0; //取得 Callback 方法
};

#ifndef KF_INTERFACE_ID_USE_GUID
#define _INTERNAL_KF_INTERFACE_ID_ASYNC_THREAD_WORKER "__kf_iid_async_thread_worker"
#else
#define _INTERNAL_KF_INTERFACE_ID_ASYNC_THREAD_WORKER "_9038CCA5588542278BC86CE88A393EAA"
#endif
struct IKFAsyncThreadWorker_I : public IKFBaseObject
{
    enum WorkItemPriority //优先级
    {
        WorkItemLevel0, //最低优先级，插到队列尾
        WorkItemLevel1, //中等优先级，插到队列中间
        WorkItemLevel2  //最高优先级，插到队列头
    };
    virtual KF_RESULT TaskQueueStartup(bool delayRunThread) = 0; //运行这个线程的任务队列
    virtual KF_RESULT TaskQueueShutdown() = 0; //关闭任务队列
    virtual KF_RESULT PutItem(IKFAsyncWorkItem_I* asyncItem, WorkItemPriority level) = 0; //提交任务到队列
    virtual int GetItemCount() = 0;
    virtual void SetTimeout(int timeout_ms) = 0;
    virtual int GetExecuteElapsedTime() = 0; //取得当前执行中的任务已经执行了多久(ms)
    
    virtual bool IsTaskQueueMoved() = 0; //判断自己的任务队列是不是已经被移走
    virtual void MoveCurrentTaskQueue(IKFAsyncThreadWorker_I* other) = 0; //把自己的任务队列移到别的Worker
    virtual bool AcceptTaskQueueMove(IKFAsyncThreadWorker_I* other, IKFArrayList* queue) = 0; //接受从别的Worker过来的任务队列
};

#ifndef KF_INTERFACE_ID_USE_GUID
#define _INTERNAL_KF_INTERFACE_ID_ASYNC_GROUP_WORKER "__kf_iid_async_group_worker"
#else
#define _INTERNAL_KF_INTERFACE_ID_ASYNC_GROUP_WORKER "_B7815B51E4FF4FCA873549F1F8FD013D"
#endif
struct IKFAsyncGroupWorker_I : public IKFBaseObject
{
    virtual KF_RESULT Startup(int max_threads, const char* name) = 0; //启动第一个 ThreadWorker 对象
    virtual KF_RESULT Shutdown() = 0; //关闭所有 ThreadWorker 对象
    virtual KF_RESULT PutWorkItem(IKFAsyncResult* result, bool realtime) = 0; //realtime 表示是否插到队列头
    virtual int GetCurrentThreads() = 0;
};

#endif //__KF_ASYNC__ASYNC_INTERNAL_H