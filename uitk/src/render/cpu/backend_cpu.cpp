// uitk_render_cpu: CPU-only backend. Contains no GPU code.
#include "../skia/skia_internal.h"

namespace uitk {

std::unique_ptr<WindowRenderer> WindowRenderer::create(Window& w) {
    return skglue::make_raster_window_renderer(w);
}

std::unique_ptr<OffscreenRenderer> OffscreenRenderer::create(Size logical, float scale) {
    return skglue::make_raster_offscreen_renderer(logical, scale);
}

} // namespace uitk
