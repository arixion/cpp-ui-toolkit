// Top-level window. On Linux the backend (Wayland or X11) is chosen at runtime:
// Wayland if WAYLAND_DISPLAY is set and reachable, else X11. UITK_PLATFORM=x11|wayland overrides.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "uitk/core/animation.h"
#include "uitk/core/event.h"
#include "uitk/core/geometry.h"

namespace uitk {

class Application;

struct WindowDesc {
    std::string title    = "uitk";
    std::string app_id   = "uitk-app";
    Size        size     = {900, 600};   // logical px
    Size        min_size = {360, 200};
};

enum class MouseCursor {
    Default, Text, Pointer, Move,
    ResizeN, ResizeS, ResizeE, ResizeW, ResizeNE, ResizeNW, ResizeSE, ResizeSW,
};

// Resize edges; values match xdg_toplevel.resize_edge.
enum ResizeEdge : unsigned {
    EDGE_NONE = 0, EDGE_TOP = 1, EDGE_BOTTOM = 2, EDGE_LEFT = 4, EDGE_RIGHT = 8,
};

// Handles a GPU renderer needs to create a surface for this window.
struct NativeSurface {
    enum class Kind { Wayland, X11 } kind;
    void*         display = nullptr;   // wl_display* / Display*
    void*         surface = nullptr;   // wl_surface* (Wayland)
    unsigned long window  = 0;         // X11 Window
    unsigned long visual_id = 0;       // X11 VisualID of the window
};

// CPU pixel buffer for software rendering: premultiplied BGRA (ARGB32 little-endian).
struct SoftwareBuffer {
    uint32_t* pixels = nullptr;
    int width = 0, height = 0;          // physical px
    int stride = 0;                     // bytes per row
};

class Window {
public:
    static std::unique_ptr<Window> create(Application& app, const WindowDesc& desc = {});
    virtual ~Window() = default;

    // ---- callbacks ----
    std::function<void(const Event&)> on_event;
    // Called when the window should paint a frame (after request_redraw, throttled to the display).
    std::function<void()> on_frame;

    // Frame time for animations. The window sets it before each on_frame; if any animation read
    // during the frame is still running, the next frame is requested automatically (vsync-paced),
    // and frames stop as soon as everything has settled.
    FrameClock& frame_clock() { return frame_clock_; }

    // ---- state ----
    virtual const char* platform_name() const = 0;   // "wayland" / "x11"
    // Size of the window itself (what the user sees as the window), logical px.
    // Event coordinates and the renderer canvas use this rectangle's top-left as origin.
    virtual Size  size() const = 0;
    virtual float scale() const = 0;                 // physical px per logical px
    virtual bool  maximized() const = 0;             // also true when tiled/fullscreen
    virtual bool  active() const = 0;

    // ---- client-side decoration margins ----
    // Transparent margin around the window for a client-drawn drop shadow. The surface is
    // enlarged by these insets; the compositor is told the real window bounds (so snapping and
    // maximize ignore the margin) and pointer input in the margin passes through, except for
    // `resize_grip` px next to the window edge. Insets are dropped while maximized/tiled.
    virtual void   set_shadow_insets(Insets insets, float resize_grip) = 0;
    virtual Insets insets() const = 0;               // currently effective (0 when maximized)

    Size surface_size() const {
        Insets i = insets();
        return {size().width + i.left + i.right, size().height + i.top + i.bottom};
    }

    // ---- requests ----
    virtual void set_title(const std::string& title) = 0;
    virtual void minimize() = 0;
    virtual void set_maximized(bool maximized) = 0;
    virtual void begin_move() = 0;                    // call from a pointer Down event
    virtual void begin_resize(unsigned edges) = 0;    // ResizeEdge bits; from a pointer Down event
    virtual void show_window_menu(Point pos) = 0;     // from a pointer Down event; no-op if unsupported
    virtual void set_cursor(MouseCursor shape) = 0;
    virtual void request_redraw() = 0;
    virtual void close() = 0;                         // destroy-on-request; emits nothing

    // ---- renderer integration ----
    virtual NativeSurface native_surface() const = 0;
    // Software path: acquire a buffer at the current physical size, draw, then present it.
    virtual SoftwareBuffer acquire_software_buffer() = 0;
    virtual void present_software_buffer() = 0;
    // GPU path: call right before the GPU swap so the window can attach its frame pacing/state.
    virtual void will_present_gpu() = 0;

    // Physical size of the whole surface (window + shadow margin).
    Size pixel_size() const {
        float s = scale();
        Size ss = surface_size();
        return {(float)(int)(ss.width * s + 0.5f), (float)(int)(ss.height * s + 0.5f)};
    }

protected:
    // Backends call this instead of on_frame directly.
    void run_frame() {
        frame_clock_.begin_frame(AnimClock::now());
        if (on_frame) on_frame();
        if (frame_clock_.wants_frame()) request_redraw();
    }

private:
    FrameClock frame_clock_;
};

} // namespace uitk
