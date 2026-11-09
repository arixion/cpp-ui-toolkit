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

std::string format_date(std::chrono::system_clock::time_point t, std::chrono::system_clock::time_point now) {
    std::time_t tt = std::chrono::system_clock::to_time_t(t), nt = std::chrono::system_clock::to_time_t(now);
    std::tm a{}, b{};
    localtime_r(&tt, &a);
    localtime_r(&nt, &b);
    char buf[32];
    if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) strftime(buf, sizeof buf, "%H:%M", &a);
    else if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday - 1) return "Yesterday";
    else if (a.tm_year == b.tm_year) strftime(buf, sizeof buf, "%b %e", &a);
    else strftime(buf, sizeof buf, "%b %e %Y", &a);
    std::string s = buf;
    // %e pads single-digit days with a space: "Nov  9" -> "Nov 9".
    for (size_t p; (p = s.find("  ")) != std::string::npos;) s.erase(p, 1);
    return s;
}

} // namespace uitk
