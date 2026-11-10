// Internal helpers shared by the Linux platform backends.
#pragma once
#include <cmath>
#include <cstdint>
#include <memory>

#include "uitk/core/event.h"
#include "uitk/platform/window.h"
#include "../common/click_tracker.h"

namespace uitk::linux_impl {

using platform::ClickTracker;

std::unique_ptr<Window> create_wayland_window(Application& app, const WindowDesc& desc);  // null if unavailable
std::unique_ptr<Window> create_x11_window(Application& app, const WindowDesc& desc);      // null if unavailable

} // namespace uitk::linux_impl
