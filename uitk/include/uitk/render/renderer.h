// Renderers. Which implementation you get depends on the backend library you link:
//   uitk_render_cpu -> CPU raster only (no GPU code in the binary)
//   uitk_render_gpu -> GPU (OpenGL ES via EGL) with automatic CPU raster fallback
#pragma once
#include <memory>
#include <string>

#include "uitk/core/geometry.h"
#include "uitk/render/canvas.h"

namespace uitk {

class Window;

class WindowRenderer {
public:
    static std::unique_ptr<WindowRenderer> create(Window& window);
    virtual ~WindowRenderer() = default;

    // Returns a canvas cleared to transparent and scaled to logical px, or nullptr if the
    // window cannot take a frame right now (skip drawing; the window will ask again).
    virtual Canvas* begin_frame() = 0;
    virtual void    end_frame() = 0;
    virtual std::string description() const = 0;   // e.g. "skia-raster", "skia-ganesh-gl (…)"
};

// Renders to an offscreen image (tests, snapshots, thumbnails).
class OffscreenRenderer {
public:
    static std::unique_ptr<OffscreenRenderer> create(Size logical_size, float scale = 1.f);
    virtual ~OffscreenRenderer() = default;

    virtual Canvas* begin_frame() = 0;
    virtual void    end_frame() = 0;
    virtual bool    write_png(const std::string& path) = 0;
    virtual std::string description() const = 0;
};

} // namespace uitk
