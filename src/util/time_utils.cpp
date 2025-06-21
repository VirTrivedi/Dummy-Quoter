#include "time_utils.h"
#include <chrono>

uint64_t getCurrentTimestamp() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint64_t convertToNanoseconds(uint64_t seconds) {
    return seconds * 1e9;
}

uint64_t convertToMilliseconds(uint64_t nanoseconds) {
    return nanoseconds / 1e6;
}