#ifndef __KF_UTIL__DBGTOOL_AVERAGE_SAMPLER_H
#define __KF_UTIL__DBGTOOL_AVERAGE_SAMPLER_H

#include <mutex>

template<typename T>
struct AverageSampler
{
    AverageSampler() throw() : _min_value(0), _max_value(0), _total_value(0), _count(0) {}
    ~AverageSampler() throw() {}

    void Submit(T value) {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        if (value < _min_value || _min_value == 0)
            _min_value = value;
        if (value > _max_value || _max_value == 0)
            _max_value = value;
        _total_value += decltype(_total_value)(value);
        _count++;
    }
    void Clear() {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        _count = 0;
        _total_value = 0;
        _min_value = _max_value = 0;
    }
    int Count()
    { std::lock_guard<decltype(_mutex)> lock(_mutex); return _count; }
    unsigned long long Total()
    { std::lock_guard<decltype(_mutex)> lock(_mutex); return _total_value; }

    T Min()
    { std::lock_guard<decltype(_mutex)> lock(_mutex); return _min_value; }
    T Max()
    { std::lock_guard<decltype(_mutex)> lock(_mutex); return _max_value; }
    T Average()
    {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        if (_count == 0)
            return T(0);
        return T(double(_total_value) / double(_count) + 0.5);
    }

private:
    std::recursive_mutex _mutex;
    T _min_value, _max_value;
    unsigned long long _total_value;
    int _count;
};

#endif //__KF_UTIL__DBGTOOL_AVERAGE_SAMPLER_H