#include "uitk/core/perf.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <time.h>
#include <unistd.h>
#endif

#include <chrono>
#include <cstdio>

namespace uitk {

namespace {

int64_t wall_now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// CPU time used by the whole process (all threads), in ns.
int64_t process_cpu_ns() {
#ifdef _WIN32
    FILETIME create, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user)) return 0;
    auto ticks = [](FILETIME f) { return ((int64_t)f.dwHighDateTime << 32) | f.dwLowDateTime; };
    return (ticks(kernel) + ticks(user)) * 100;   // FILETIME: 100 ns units
#else
    timespec ts{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (int64_t)ts.tv_sec * 1'000'000'000 + ts.tv_nsec;
#endif
}

} // namespace

CpuMeter::CpuMeter() : cpu_ns_(process_cpu_ns()), wall_ns_(wall_now_ns()) {}

float CpuMeter::sample() {
    int64_t cpu = process_cpu_ns(), wall = wall_now_ns();
    int64_t dw = wall - wall_ns_;
    if (dw > 0) last_ = (float)(100.0 * (double)(cpu - cpu_ns_) / (double)dw);
    cpu_ns_ = cpu;
    wall_ns_ = wall;
    return last_;
}

int CpuMeter::core_count() {
#ifdef _WIN32
    DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return n > 0 ? (int)n : 1;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}

uint64_t resident_memory_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) return 0;
    return (uint64_t)pmc.WorkingSetSize;
#else
    FILE* f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    unsigned long size = 0, resident = 0;
    int n = fscanf(f, "%lu %lu", &size, &resident);
    fclose(f);
    return n == 2 ? (uint64_t)resident * (uint64_t)sysconf(_SC_PAGESIZE) : 0;
#endif
}

} // namespace uitk
