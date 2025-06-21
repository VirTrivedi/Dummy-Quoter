#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <cstdint>
#include <chrono>

namespace TimeUtils {

    uint64_t current_time_nanos() {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }

    uint64_t time_since_epoch_nanos() {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }

}

#endif