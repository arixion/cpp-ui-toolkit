// Turns button presses into click counts (single, double, triple ...) for PointerEvent.
// Shared by every platform backend.
#pragma once
#include <cmath>
#include <cstdint>

#include "uitk/core/event.h"

namespace uitk::platform {

struct ClickTracker {
    uint32_t interval_ms = 400;   // max time between presses (Windows: GetDoubleClickTime)
    float    slop = 4;            // max distance in logical px
    uint32_t last_time = 0;
    Point    last_pos;
    PointerButton last_button = PointerButton::NoButton;
    int      count = 0;

    int press(PointerButton b, Point p, uint32_t time_ms) {
        bool same = b == last_button && time_ms - last_time <= interval_ms &&
                    std::fabs(p.x - last_pos.x) <= slop && std::fabs(p.y - last_pos.y) <= slop;
        count = same ? count + 1 : 1;
        last_time = time_ms; last_pos = p; last_button = b;
        return count;
    }
};

} // namespace uitk::platform
