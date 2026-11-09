// Runtime selection of the Linux windowing backend.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "linux_common.h"

namespace uitk {

std::unique_ptr<Window> Window::create(Application& app, const WindowDesc& desc) {
    using namespace linux_impl;
    const char* forced = getenv("UITK_PLATFORM");
    if (forced && !strcmp(forced, "x11")) return create_x11_window(app, desc);
    if (forced && !strcmp(forced, "wayland")) return create_wayland_window(app, desc);
    if (forced && *forced) fprintf(stderr, "uitk: unknown UITK_PLATFORM=%s, auto-detecting\n", forced);

    if (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET"))
        if (auto w = create_wayland_window(app, desc)) return w;
    return create_x11_window(app, desc);
}

} // namespace uitk
