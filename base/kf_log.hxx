#ifndef __KF_BASE__LOG_H
#define __KF_BASE__LOG_H

#include <cstdarg>

#define KFLOG(format, ...)        kGlobalLogger.Printf(KFLogger::LogLevel::LevelVerbose, format, __VA_ARGS__);
#define KFLOG_INFO(format, ...)   kGlobalLogger.Printf(KFLogger::LogLevel::LevelInfo, format, __VA_ARGS__);
#define KFLOG_WARN(format, ...)   kGlobalLogger.Printf(KFLogger::LogLevel::LevelWarning, format, __VA_ARGS__);
#define KFLOG_ERROR(format, ...)  kGlobalLogger.Printf(KFLogger::LogLevel::LevelError, format, __VA_ARGS__);
#define KFLOG_TRACE(format, ...)  kGlobalLogger.Printf(KFLogger::LogLevel::LevelInternalTrace, format, __VA_ARGS__);
#define KFLOG_DEBUG(format, ...)  kGlobalLogger.Printf(KFLogger::LogLevel::LevelInternalDebug, format, __VA_ARGS__);

#define KFLOG_T(format, ...) \
kGlobalLogger.Printf(KFLogger::LogLevel::LevelVerbose, "(%s) " format " (Line: %d)", (KF_LOG_TAG_STR), __VA_ARGS__, __LINE__);
#define KFLOG_INFO_T(format, ...) \
kGlobalLogger.Printf(KFLogger::LogLevel::LevelInfo, "(%s) " format " (Line: %d)", (KF_LOG_TAG_STR), __VA_ARGS__, __LINE__);
#define KFLOG_WARN_T(format, ...) \
kGlobalLogger.Printf(KFLogger::LogLevel::LevelWarning, "(%s) " format " (Line: %d)", (KF_LOG_TAG_STR), __VA_ARGS__, __LINE__);
#define KFLOG_ERROR_T(format, ...) \
kGlobalLogger.Printf(KFLogger::LogLevel::LevelError, "(%s) " format " (Line: %d)", (KF_LOG_TAG_STR), __VA_ARGS__, __LINE__);
#define KFLOG_TRACE_T(format, ...) \
kGlobalLogger.Printf(KFLogger::LogLevel::LevelInternalTrace, "(%s) " format " (Line: %d)", (KF_LOG_TAG_STR), __VA_ARGS__, __LINE__);
#define KFLOG_DEBUG_T(format, ...) \
kGlobalLogger.Printf(KFLogger::LogLevel::LevelInternalDebug, "(%s) " format " (Line: %d)", (KF_LOG_TAG_STR), __VA_ARGS__, __LINE__);

#define KFLOG_ENABLE_LEVEL(x) \
kGlobalLogger.SetLevel(x);

struct KFLogger
{
    typedef void (*LoggerCallback)(const char* log);

    enum LogLevel
    {
        LevelVerbose = 0,
        LevelInfo = 1,
        LevelWarning = 2,
        LevelError = 3,
        LevelInternalTrace = 4,
        LevelInternalDebug = 5
    };
    struct PrintTarget
    {
        PrintTarget() {}
        virtual ~PrintTarget() {}

        virtual void Printf(LogLevel level, const char* format) = 0;
        virtual void Clear() {}
    };

    KFLogger(PrintTarget* pt, bool thread_safe = false) throw();
    ~KFLogger() throw();

    void Printf(LogLevel level, const char* format, ...);
    void SetState(bool state) { _state = state; }
    void SetCallback(LoggerCallback callback) { _callback = callback; }
    void SetLevel(LogLevel level) throw() { _max_level = level; }
    LogLevel GetLevel() const throw() { return _max_level; }

private:
    static bool CheckLogCond(LogLevel level);

    bool _state;
    PrintTarget* _printf;
    LoggerCallback _callback;
    LogLevel _max_level;
    void* _std_mutex;
};

extern KFLogger kGlobalLogger;

#endif //__KF_BASE__LOG_H