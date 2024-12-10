#include "kf_log.hxx"
#include <stdlib.h>
#include <string.h>
#include <mutex>

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#endif

#ifdef _MSC_VER
#include <Windows.h>
#pragma warning(disable:4996)
#pragma warning(disable:4100)
#endif

#define MAX_DEFAULT_PRINT_LEN 2048
static const char* kLogDebugStr[] = {" [verbose]\n", " [info]\n", " [warning]\n", " [error]\n", " [trace]\n", " [debug]\n"};

bool KFLogger::CheckLogCond(LogLevel level)
{
#ifdef KF_LOG_INFO_ONLY
    return (level == LogLevel::LevelInfo);
#elif KF_LOG_ERROR_ONLY
    return (level == LogLevel::LevelError);
#elif KF_LOG_INTERNAL_ONLY
    return (level == LogLevel::LevelInternalTrace || level == LogLevel::LevelInternalDebug);
#endif
    return true;
}

KFLogger::KFLogger(PrintTarget * pt, bool thread_safe) throw() : _printf(pt), _std_mutex(nullptr)
{
    _state = true;
    _callback = nullptr;
    _max_level = LogLevel::LevelVerbose;
    if (thread_safe)
        _std_mutex = new(std::nothrow) std::recursive_mutex();
}

KFLogger::~KFLogger() throw()
{
    delete _printf;
    if (_std_mutex)
        delete (std::recursive_mutex*)_std_mutex;
}

void KFLogger::Printf(LogLevel level, const char * format, ...)
{
#ifndef KF_LOG_SKIP_ALL
    if (!_state) //Disabled
        return;
    if ((int)level < (int)_max_level)
        return;
    if (!CheckLogCond(level))
        return;

    if (_std_mutex)
        ((std::recursive_mutex*)_std_mutex)->lock();

    if (format) {
        char buffer[MAX_DEFAULT_PRINT_LEN];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, MAX_DEFAULT_PRINT_LEN, format, args);
        _printf->Printf(level, buffer);
        if (_callback)
            _callback(buffer);
        va_end(args);
    }

    if (_std_mutex)
        ((std::recursive_mutex*)_std_mutex)->unlock();
#endif
}

//******* Platform *******

struct CommonPrintf : public KFLogger::PrintTarget
{
    virtual void Printf(KFLogger::LogLevel level, const char* format)
    {
#if (defined(KF_LOG_INFO_ONLY) || defined(KF_LOG_ERROR_ONLY) || defined(KF_LOG_INTERNAL_ONLY))
        printf(format);
        return;
#endif
        int index = (int)level;
        auto str = (char*)malloc(strlen(format) * 2);
        strcpy(str, format);
        strcat(str, kLogDebugStr[index]);
#ifdef __APPLE__
        NSLog(@"%s", str);
#else
        printf("%s", str);
#endif
        free(str);
    }
};

#ifdef _MSC_VER
struct WindowsPrintf : public KFLogger::PrintTarget
{
    virtual void Printf(KFLogger::LogLevel level, const char* format)
    {
#if (defined(KF_LOG_INFO_ONLY) || defined(KF_LOG_ERROR_ONLY) || defined(KF_LOG_INTERNAL_ONLY))
        OutputDebugStringA(format);
        return;
#endif
        int index = (int)level;
        auto str = (char*)malloc(strlen(format) * 2);
        strcpy(str, format);
        strcat(str, kLogDebugStr[index]);
        OutputDebugStringA(str);
        free(str);
    }
};
#endif

#ifdef _MSC_VER
KFLogger kGlobalLogger(new(std::nothrow) WindowsPrintf(), true);
#else
KFLogger kGlobalLogger(new(std::nothrow) CommonPrintf(), true);
#endif