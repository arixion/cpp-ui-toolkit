#include "uitk/core/perf.h"

#include <time.h>
#include <unistd.h>

#include <cstdio>

namespace uitk {

namespace {
int64_t now_ns(clockid_t clock) {
    timespec ts{};
    clock_gettime(clock, &ts);
    return (int64_t)ts.tv_sec * 1'000'000'000 + ts.tv_nsec;
}
} // namespace

CpuMeter::CpuMeter()
    : cpu_ns_(now_ns(CLOCK_PROCESS_CPUTIME_ID)), wall_ns_(now_ns(CLOCK_MONOTONIC)) {}

float CpuMeter::sample() {
    int64_t cpu = now_ns(CLOCK_PROCESS_CPUTIME_ID), wall = now_ns(CLOCK_MONOTONIC);
    int64_t dw = wall - wall_ns_;
    if (dw > 0) last_ = (float)(100.0 * (double)(cpu - cpu_ns_) / (double)dw);
    cpu_ns_ = cpu;
    wall_ns_ = wall;
    return last_;
}

int CpuMeter::core_count() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

uint64_t resident_memory_bytes() {
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    unsigned long size = 0, resident = 0;
    int n = fscanf(f, "%lu %lu", &size, &resident);
    fclose(f);
    return n == 2 ? (uint64_t)resident * (uint64_t)sysconf(_SC_PAGESIZE) : 0;
}

} // namespace uitk
