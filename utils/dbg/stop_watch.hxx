#ifndef __KF_UTIL__DBGTOOL_STOP_WATCH_H
#define __KF_UTIL__DBGTOOL_STOP_WATCH_H

#include <sys/kf_sys_platform.h>

struct StopWatch
{
    StopWatch() throw() : _running(false), _start_time(0), _end_time(0) {}
    ~StopWatch() throw() {}

    bool Start() {
        if (_running)
            return false;

        _start_time = KFGetTick();
        _end_time = 0;
        _running = true;
        return true;
    }

    bool Stop() {
        if (!_running)
            return false;

        _end_time = KFGetTick();
        _running = false;
        return true;
    }
    
    bool Restart() {
        if (!_running)
            return Start();

        _start_time = KFGetTick();
        _end_time = 0;
        return true;
    }

    void Reset() {
        _running = false;
        _start_time = _end_time = 0;
    }

    bool IsRunning() {
        return _running;
    }

    int Elapsed() {
        if (_end_time == 0)
            return -1;
        return int(_end_time - _start_time);
    }

private:
    bool _running;
    long long _start_time, _end_time;
};

#endif //__KF_UTIL__DBGTOOL_STOP_WATCH_H