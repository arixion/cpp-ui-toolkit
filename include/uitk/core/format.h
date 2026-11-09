// Human-readable formatting helpers shared by widgets (file sizes, dates).
#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace uitk {

// "512 bytes", "4.1 kB", "12.3 MB" (SI units, as GNOME and macOS show file sizes).
std::string format_bytes(uint64_t bytes);

// Compact file-manager style: "14:05" (today), "Yesterday", "Nov 9" (this year), "Nov 9 2024".
std::string format_date(std::chrono::system_clock::time_point t,
                        std::chrono::system_clock::time_point now = std::chrono::system_clock::now());

} // namespace uitk
