// Lightweight process performance metrics.
#pragma once
#include <cstdint>

namespace uitk {

// Process CPU usage between successive sample() calls, as a percentage of ONE core
// (like `top`): 100% = one core fully busy; can exceed 100% with several busy threads.
class CpuMeter {
public:
    CpuMeter();
    float sample();                  // % since the previous sample (or construction)
    float last() const { return last_; }
    static int core_count();

private:
    int64_t cpu_ns_  = 0;            // process CPU time at the previous sample
    int64_t wall_ns_ = 0;            // monotonic time at the previous sample
    float   last_    = 0;
};

// Resident memory of this process in bytes (0 if unavailable).
uint64_t resident_memory_bytes();

} // namespace uitk
