#include "logger_fileio.hxx"

KF_RESULT DbgFileLogger::Start(const char* file, LoggerOutputCallback outputCB)
{
    _logCount = 0;
    _outputCB = outputCB;
    _file = nullptr;

    if (file) {
        _file = KFileCreate(FILE_ACCESS_READWRITE, OPEN_MODE_DELETE_IF_EXIST, file);
        if (_file == nullptr)
            return KF_ACCESS_DENIED;
    }

    return KFAsyncCreateWorker(false, 1, &_worker);
}

KF_RESULT DbgFileLogger::Stop(bool flush)
{
    if (flush) {
        while (_logCount != 0) {
            KFSleep(10);
        }
    }else{
        _worker.Destroy();
    }

    if (_file) {
        KFileFlush(_file);
        KFileClose(_file);
    }
    return KF_OK;
}

KF_RESULT DbgFileLogger::Post(const char* str, int* tick)
{
    auto time = KFGetTick();
    auto strLen = (int)strlen(str);

    KFPtr<IKFBuffer> buf;
    auto result = KFCreateMemoryBufferFast(strLen + 1, &buf);
    _KF_FAILED_RET(result);
    strcpy(KFGetBufferAddress<char*>(buf.Get()), str);
    buf->SetCurrentLength(strLen);

    result = KFAsyncPutWorkItem(_worker.Get(), &_callback, buf.Get());
    _KF_FAILED_RET(result);

    if (tick)
        *tick = int(KFGetTick() - time);

    _KFRefInc(&_logCount);
    return KF_OK;
}

void DbgFileLogger::OnInvoke(IKFAsyncResult* result)
{
    KFPtr<IKFBuffer> log;
    KFBaseGetInterface(result->GetStateNoRef(), _KF_INTERFACE_ID_BUFFER, &log);
    if (_file)
        KFileWrite(_file, log->GetAddress(), log->GetCurrentLength());
    if (_outputCB)
        _outputCB(KFGetBufferAddress<char*>(log.Get()));
    _KFRefDec(&_logCount);
}