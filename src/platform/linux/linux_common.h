// Internal helpers shared by the Linux platform backends.
#pragma once
#include <cmath>
#include <cstdint>
#include <memory>

#include "uitk/core/event.h"
#include "uitk/platform/window.h"

namespace uitk::linux_impl {

// Tracks consecutive clicks to produce PointerEvent::click_count.
struct ClickTracker {
    uint32_t last_time = 0;
    Point    last_pos;
    PointerButton last_button = PointerButton::NoButton;
    int      count = 0;

    int press(PointerButton b, Point p, uint32_t time_ms) {
        bool same = b == last_button && time_ms - last_time <= 400 &&
                    std::fabs(p.x - last_pos.x) <= 4 && std::fabs(p.y - last_pos.y) <= 4;
        count = same ? count + 1 : 1;
        last_time = time_ms; last_pos = p; last_button = b;
        return count;
    }
};

std::unique_ptr<Window> create_wayland_window(Application& app, const WindowDesc& desc);  // null if unavailable
std::unique_ptr<Window> create_x11_window(Application& app, const WindowDesc& desc);      // null if unavailable

} // namespace uitk::linux_impl
