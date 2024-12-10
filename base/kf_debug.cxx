#include <base/kf_debug.hxx>
#include <base/kf_log.hxx>

KFLogCallback kLogCallback;

static void ThunkCallback(const char* log)
{
    if (kLogCallback)
        kLogCallback(log);
}

void KFAPI KFDbgLogSetCallback(KFLogCallback callback)
{
    if (callback == nullptr) {
        kGlobalLogger.SetCallback(nullptr);
        kLogCallback = nullptr;
    }else{
        kGlobalLogger.SetCallback(&ThunkCallback);
        kLogCallback = callback;
    }
}

void KFAPI KFDbgLogSetLevel(int level)
{
    switch (level) {
    case KF_LOG_LEVEL_VERBOSE:
        kGlobalLogger.SetLevel(KFLogger::LogLevel::LevelVerbose);
        break;
    case KF_LOG_LEVEL_INFO:
        kGlobalLogger.SetLevel(KFLogger::LogLevel::LevelInfo);
        break;
    case KF_LOG_LEVEL_WARNING:
        kGlobalLogger.SetLevel(KFLogger::LogLevel::LevelWarning);
        break;
    case KF_LOG_LEVEL_ERROR:
        kGlobalLogger.SetLevel(KFLogger::LogLevel::LevelError);
        break;
    case KF_LOG_LEVEL_I_TRACE:
        kGlobalLogger.SetLevel(KFLogger::LogLevel::LevelInternalTrace);
        break;
    case KF_LOG_LEVEL_I_DEBUG:
        kGlobalLogger.SetLevel(KFLogger::LogLevel::LevelInternalDebug);
        break;
    }
}

void KFAPI KFDbgLogOn()
{
    kGlobalLogger.SetState(true);
}

void KFAPI KFDbgLogOff()
{
    kGlobalLogger.SetState(false);
}