#include "uitk/core/format.h"

#include <cstdio>
#include <ctime>

namespace uitk {

std::string format_bytes(uint64_t bytes) {
    char buf[32];
    if (bytes < 1000) {
        snprintf(buf, sizeof buf, bytes == 1 ? "%llu byte" : "%llu bytes", (unsigned long long)bytes);
        return buf;
    }
    static const char* units[] = {"kB", "MB", "GB", "TB", "PB"};
    double v = (double)bytes / 1000.0;
    int u = 0;
    while (v >= 1000.0 && u < 4) { v /= 1000.0; ++u; }
    snprintf(buf, sizeof buf, v < 10 ? "%.1f %s" : "%.0f %s", v, units[u]);
    return buf;
}

namespace {
std::tm local_tm(std::time_t t) {
    std::tm r{};
#ifdef _WIN32
    localtime_s(&r, &t);
#else
    localtime_r(&t, &r);
#endif
    return r;
}
} // namespace

std::string format_date(std::chrono::system_clock::time_point t, std::chrono::system_clock::time_point now) {
    std::tm a = local_tm(std::chrono::system_clock::to_time_t(t));
    std::tm b = local_tm(std::chrono::system_clock::to_time_t(now));
    char buf[32];
    if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) { strftime(buf, sizeof buf, "%H:%M", &a); return buf; }
    if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday - 1) return "Yesterday";
    // "Nov 9" / "Nov 9 2025" (%e is not portable; MSVC aborts on it).
    char mon[16];
    strftime(mon, sizeof mon, "%b", &a);
    if (a.tm_year == b.tm_year) snprintf(buf, sizeof buf, "%s %d", mon, a.tm_mday);
    else snprintf(buf, sizeof buf, "%s %d %d", mon, a.tm_mday, a.tm_year + 1900);
    return buf;
}

} // namespace uitk
