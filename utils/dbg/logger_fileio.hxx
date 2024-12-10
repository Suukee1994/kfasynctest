#ifndef __KF_UTIL__DBGTOOL_LOGGER_FILEIO_H
#define __KF_UTIL__DBGTOOL_LOGGER_FILEIO_H

#include <base/kf_log.hxx>
#include <base/kf_ptr.hxx>
#include <base/kf_buffer.hxx>
#include <sys/kf_sys_fileio.h>
#include <sys/kf_sys_platform.h>
#include <async/kf_async_abstract.hxx>
#include <utils/async_callback_router.hxx>

class DbgFileLogger : private IKFBaseObject
{
    KF_IMPL_DECL_REFCOUNT;
public:
    static DbgFileLogger* CreateDbgFileLogger() throw()
    { return new(std::nothrow) DbgFileLogger(); }

    typedef void (*LoggerOutputCallback)(const char*);

private:
    DbgFileLogger() throw() : _ref_count(1)
    { _callback.SetCallback(this, &DbgFileLogger::OnInvoke); }
    virtual ~DbgFileLogger() throw() {}

public:
    virtual KF_RESULT CastToInterface(KIID, void**)
    { return KF_NO_INTERFACE; }
    virtual KREF Retain()
    { KF_IMPL_RETAIN_FUNC(_ref_count); }
    virtual KREF Recycle()
    { KF_IMPL_RECYCLE_FUNC(_ref_count); }

public:
    KF_RESULT Start(const char* file, LoggerOutputCallback outputCB = nullptr);
    KF_RESULT Stop(bool flush = true);

    KF_RESULT Post(const char* str, int* tick = nullptr);

private:
    void OnInvoke(IKFAsyncResult* result);

    KFILE _file;
    LoggerOutputCallback _outputCB;
    KFAsyncWorker _worker;
    AsyncCallbackRouter<DbgFileLogger> _callback;
    KREF _logCount;
};

#endif //__KF_UTIL__DBGTOOL_LOGGER_FILEIO_H