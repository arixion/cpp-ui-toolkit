// Wayland backend: xdg-shell toplevel with client-side decorations.
// - wl_shm double buffers for software rendering; GPU renderers attach via native_surface().
// - Frame pacing with wl_surface.frame callbacks (no drawing while hidden/minimized).
// - Integer HiDPI via wl_surface.preferred_buffer_scale (v6) or wl_output scale fallback.
// - Keyboard through xkbcommon (keysyms, UTF-8 text, client-side key repeat).
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "uitk/core/application.h"
#include "../linux_common.h"

namespace uitk::linux_impl {
namespace {

class WaylandWindow;

// ============================================================================ display (per process)

struct Output { wl_output* output; uint32_t name; int scale = 1; };

class WaylandDisplay final : public EventSource {
public:
    static WaylandDisplay* get(Application& app);   // connects lazily; null if no compositor

    int  fd() const override { return wl_display_get_fd(display); }
    void prepare() override;
    void dispatch(short revents) override;

    wl_display*    display = nullptr;
    wl_registry*   registry = nullptr;
    wl_compositor* compositor = nullptr;
    uint32_t       compositor_version = 0;
    wl_shm*        shm = nullptr;
    wl_seat*       seat = nullptr;
    wl_pointer*    pointer = nullptr;
    wl_keyboard*   keyboard = nullptr;
    xdg_wm_base*   wm_base = nullptr;
    zxdg_decoration_manager_v1* deco_manager = nullptr;
    std::vector<Output> outputs;

    Application* app = nullptr;
    std::vector<WaylandWindow*> windows;

    // pointer
    WaylandWindow* pointer_focus = nullptr;
    uint32_t pointer_enter_serial = 0;
    uint32_t last_input_serial = 0;      // for move/resize/window-menu requests
    Point    pointer_pos;
    ClickTracker clicks;
    wl_surface* cursor_surface = nullptr;
    std::map<int, wl_cursor_theme*> cursor_themes;   // by scale
    MouseCursor cursor_shape = MouseCursor::Default;

    // keyboard
    WaylandWindow* keyboard_focus = nullptr;
    xkb_context* xkb_ctx = nullptr;
    xkb_keymap*  xkb_keymap_ = nullptr;
    xkb_state*   xkb_state_ = nullptr;
    uint32_t mods = MOD_NONE;
    int32_t  repeat_rate = 25, repeat_delay = 600;
    uint32_t repeat_key = 0;
    TimerId  repeat_timer = 0;

    bool prepared = false;            // wl_display_prepare_read() is outstanding

    void apply_cursor(int scale);
    void emit_key(uint32_t key, bool pressed, bool repeat);
    void stop_repeat();
    WaylandWindow* window_for(wl_surface* s);
};

// ============================================================================ window

struct ShmBuffer {
    WaylandWindow* owner = nullptr;
    wl_buffer* buffer = nullptr;
    uint32_t*  data = nullptr;
    int width = 0, height = 0;
    bool busy = false;
    // Backing memory, kept across resizes: during an interactive resize only the wl_buffer is
    // recreated, avoiding a new memfd + mmap + fresh page faults on every configure.
    int          fd = -1;
    wl_shm_pool* pool = nullptr;
    size_t       capacity = 0;
};

class WaylandWindow final : public Window {
public:
    WaylandWindow(WaylandDisplay* d, Application& app, const WindowDesc& desc);
    ~WaylandWindow() override;

    const char* platform_name() const override { return "wayland"; }
    Size  size() const override { return size_; }
    Insets insets() const override { return maximized_ ? Insets{} : requested_insets_; }
    void set_shadow_insets(Insets i, float grip) override {
        requested_insets_ = i; grip_ = grip;
        request_redraw();
    }
    float scale() const override { return (float)scale_; }
    bool  maximized() const override { return maximized_; }
    bool  active() const override { return active_; }

    void set_title(const std::string& t) override { if (toplevel) xdg_toplevel_set_title(toplevel, t.c_str()); }
    void minimize() override { if (toplevel) xdg_toplevel_set_minimized(toplevel); }
    void set_maximized(bool m) override {
        if (!toplevel) return;
        if (m) xdg_toplevel_set_maximized(toplevel); else xdg_toplevel_unset_maximized(toplevel);
    }
    void begin_move() override {
        if (toplevel) xdg_toplevel_move(toplevel, d->seat, d->last_input_serial);
    }
    void begin_resize(unsigned edges) override {
        // ResizeEdge bits are identical to xdg_toplevel_resize_edge values.
        if (toplevel) xdg_toplevel_resize(toplevel, d->seat, d->last_input_serial, edges);
    }
    void show_window_menu(Point p) override {
        Insets i = insets();   // the request takes surface-local coordinates
        if (toplevel) xdg_toplevel_show_window_menu(toplevel, d->seat, d->last_input_serial,
                                                    (int)(p.x + i.left), (int)(p.y + i.top));
    }
    void set_cursor(MouseCursor s) override {
        if (s == cursor_) return;
        cursor_ = s;
        if (d->pointer_focus == this) { d->cursor_shape = s; d->apply_cursor(scale_); }
    }
    void request_redraw() override;
    void close() override;

    NativeSurface native_surface() const override {
        NativeSurface ns{NativeSurface::Kind::Wayland};
        ns.display = d->display; ns.surface = surface;
        return ns;
    }
    SoftwareBuffer acquire_software_buffer() override;
    void present_software_buffer() override;
    void will_present_gpu() override { before_commit(); }

    // ---- internal ----
    void emit(const Event& e) { if (on_event) on_event(e); }
    void before_commit();
    void paint_now();
    void update_scale();

    WaylandDisplay* d;
    Application& app;
    wl_surface*   surface = nullptr;
    xdg_surface*  xsurface = nullptr;
    xdg_toplevel* toplevel = nullptr;
    zxdg_toplevel_decoration_v1* deco = nullptr;

    Size  size_;
    Size  min_size_;
    Insets requested_insets_;
    float grip_ = 0;
    int   scale_ = 1;
    int   preferred_scale_ = 0;          // from wl_surface.preferred_buffer_scale (0 = unknown)
    std::vector<wl_output*> entered_outputs;
    bool  maximized_ = false, active_ = false;
    MouseCursor cursor_ = MouseCursor::Default;

    // pending configure
    int  pending_w = 0, pending_h = 0;
    bool pending_max = false, pending_active = false;
    bool configured = false;

    // frame pacing
    wl_callback* frame_cb = nullptr;
    bool dirty = false;
    bool paint_scheduled = false;

    ShmBuffer buffers[2];
    ShmBuffer* current = nullptr;
};

// ---------------------------------------------------------------------------- shm

int create_shm_file(size_t size) {
    int fd = memfd_create("uitk-wl-buffer", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) return -1;
    int r;
    do { r = ftruncate(fd, (off_t)size); } while (r < 0 && errno == EINTR);
    if (r < 0) { close(fd); return -1; }
    return fd;
}

void destroy_buffer(ShmBuffer& b) {
    if (b.buffer) wl_buffer_destroy(b.buffer);
    if (b.pool) wl_shm_pool_destroy(b.pool);
    if (b.data) munmap(b.data, b.capacity);
    if (b.fd >= 0) close(b.fd);
    b = ShmBuffer{};
}

void on_buffer_release(void* data, wl_buffer*) {
    auto* b = static_cast<ShmBuffer*>(data);
    b->busy = false;
    if (b->owner->dirty && !b->owner->frame_cb) b->owner->request_redraw();
}
const wl_buffer_listener release_listener = { .release = on_buffer_release };

bool create_buffer(WaylandDisplay* d, WaylandWindow* w, ShmBuffer& b, int width, int height) {
    int stride = width * 4;
    size_t need = (size_t)stride * height;
    // Much smaller than the pool (window shrank a lot): give the memory back.
    if (b.capacity > 4 * need && b.capacity > (8u << 20)) destroy_buffer(b);
    if (b.buffer) { wl_buffer_destroy(b.buffer); b.buffer = nullptr; }

    if (need > b.capacity) {
        // 50% headroom so a window being dragged larger reuses the pool for many steps.
        size_t page = 4096, cap = ((need + need / 2) + page - 1) / page * page;
        if (b.fd < 0) {
            b.fd = create_shm_file(cap);
            if (b.fd < 0) { perror("uitk: memfd"); return false; }
            b.pool = wl_shm_create_pool(d->shm, b.fd, (int32_t)cap);
        } else {
            int r;
            do { r = ftruncate(b.fd, (off_t)cap); } while (r < 0 && errno == EINTR);
            if (r < 0) { perror("uitk: ftruncate"); destroy_buffer(b); return false; }
            wl_shm_pool_resize(b.pool, (int32_t)cap);   // pools can only grow
            munmap(b.data, b.capacity);
        }
        void* data = mmap(nullptr, cap, PROT_READ | PROT_WRITE, MAP_SHARED, b.fd, 0);
        if (data == MAP_FAILED) { perror("uitk: mmap"); b.data = nullptr; destroy_buffer(b); return false; }
        b.data = static_cast<uint32_t*>(data);
        b.capacity = cap;
    }
    b.buffer = wl_shm_pool_create_buffer(b.pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    b.width = width; b.height = height;
    b.owner = w;
    wl_buffer_add_listener(b.buffer, &release_listener, &b);
    return true;
}

// ---------------------------------------------------------------------------- frame pacing

void on_frame_done(void* data, wl_callback* cb, uint32_t) {
    auto* w = static_cast<WaylandWindow*>(data);
    wl_callback_destroy(cb);
    w->frame_cb = nullptr;
    if (w->dirty) w->request_redraw();
}
const wl_callback_listener frame_listener = { .done = on_frame_done };

void WaylandWindow::request_redraw() {
    dirty = true;
    if (frame_cb || paint_scheduled || !configured) return;   // frame callback will pick it up
    paint_scheduled = true;
    app.post([this] { paint_scheduled = false; paint_now(); });
}

void WaylandWindow::paint_now() {
    if (!dirty || !configured || !surface) return;
    dirty = false;
    run_frame();   // sets the frame clock, calls on_frame, keeps animations going
    if (app.loop_mode() == LoopMode::Continuous) dirty = true;   // next frame callback repaints
}

void WaylandWindow::before_commit() {
    wl_surface_set_buffer_scale(surface, scale_);
    // The window geometry excludes the shadow margin: the compositor snaps/maximizes/tiles
    // using it, and configure sizes refer to it.
    Insets in = insets();
    xdg_surface_set_window_geometry(xsurface, (int)in.left, (int)in.top, (int)size_.width, (int)size_.height);
    // Input only over the window plus a resize grip; clicks on the shadow fall through.
    if (in.empty()) {
        wl_surface_set_input_region(surface, nullptr);   // whole surface
    } else {
        Size ss = surface_size();
        float g = grip_;
        int x0 = (int)std::max(0.f, in.left - g), y0 = (int)std::max(0.f, in.top - g);
        int x1 = (int)std::min(ss.width, in.left + size_.width + g);
        int y1 = (int)std::min(ss.height, in.top + size_.height + g);
        wl_region* r = wl_compositor_create_region(d->compositor);
        wl_region_add(r, x0, y0, x1 - x0, y1 - y0);
        wl_surface_set_input_region(surface, r);
        wl_region_destroy(r);
    }
    if (!frame_cb) {
        frame_cb = wl_surface_frame(surface);
        wl_callback_add_listener(frame_cb, &frame_listener, this);
    }
}

SoftwareBuffer WaylandWindow::acquire_software_buffer() {
    Size ss = surface_size();
    int pw = (int)ss.width * scale_, ph = (int)ss.height * scale_;
    current = nullptr;
    for (ShmBuffer& b : buffers) {
        if (b.busy) continue;
        if ((b.width != pw || b.height != ph) && !create_buffer(d, this, b, pw, ph)) return {};
        current = &b;
        break;
    }
    if (!current) { dirty = true; return {}; }   // both held by the compositor; retry on release
    return {current->data, current->width, current->height, current->width * 4};
}

void WaylandWindow::present_software_buffer() {
    if (!current) return;
    before_commit();
    wl_surface_attach(surface, current->buffer, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, current->width, current->height);
    wl_surface_commit(surface);
    current->busy = true;
    current = nullptr;
}

void WaylandWindow::update_scale() {
    int s = preferred_scale_;
    if (s <= 0) {
        s = 1;
        for (wl_output* o : entered_outputs)
            for (auto& out : d->outputs) if (out.output == o) s = std::max(s, out.scale);
    }
    if (s == scale_) return;
    scale_ = s;
    if (d->pointer_focus == this) d->apply_cursor(scale_);
    emit(ResizeEvent{size_, (float)scale_});
    request_redraw();
}

// ---------------------------------------------------------------------------- surface / xdg listeners

const wl_surface_listener surface_listener = {
    .enter = [](void* data, wl_surface*, wl_output* o) {
        auto* w = static_cast<WaylandWindow*>(data);
        w->entered_outputs.push_back(o);
        w->update_scale();
    },
    .leave = [](void* data, wl_surface*, wl_output* o) {
        auto* w = static_cast<WaylandWindow*>(data);
        auto& v = w->entered_outputs;
        v.erase(std::remove(v.begin(), v.end(), o), v.end());
        w->update_scale();
    },
    .preferred_buffer_scale = [](void* data, wl_surface*, int32_t factor) {
        auto* w = static_cast<WaylandWindow*>(data);
        w->preferred_scale_ = factor;
        w->update_scale();
    },
    .preferred_buffer_transform = [](void*, wl_surface*, uint32_t) {},
};

const xdg_surface_listener xsurface_listener = {
    .configure = [](void* data, xdg_surface* xs, uint32_t serial) {
        auto* w = static_cast<WaylandWindow*>(data);
        xdg_surface_ack_configure(xs, serial);
        Size ns = w->size_;
        if (w->pending_w > 0) ns.width  = std::max((float)w->pending_w, w->min_size_.width);
        if (w->pending_h > 0) ns.height = std::max((float)w->pending_h, w->min_size_.height);
        bool first = !w->configured;
        w->configured = true;
        if (first || ns != w->size_) { w->size_ = ns; w->emit(ResizeEvent{ns, (float)w->scale_}); }
        if (first || w->pending_max != w->maximized_) {
            w->maximized_ = w->pending_max; w->emit(StateEvent{w->maximized_});
        }
        if (first || w->pending_active != w->active_) {
            w->active_ = w->pending_active; w->emit(FocusEvent{w->active_});
        }
        // A configure must be answered with a new commit: paint immediately, bypassing pacing.
        w->dirty = true;
        if (w->frame_cb) { wl_callback_destroy(w->frame_cb); w->frame_cb = nullptr; }
        w->request_redraw();
    },
};

const xdg_toplevel_listener toplevel_listener = {
    .configure = [](void* data, xdg_toplevel*, int32_t width, int32_t height, wl_array* states) {
        auto* w = static_cast<WaylandWindow*>(data);
        w->pending_w = width; w->pending_h = height;
        w->pending_max = false; w->pending_active = false;
        for (auto* s = static_cast<uint32_t*>(states->data);
             (char*)s < (char*)states->data + states->size; ++s) {
            switch (*s) {
                case XDG_TOPLEVEL_STATE_MAXIMIZED:
                case XDG_TOPLEVEL_STATE_FULLSCREEN:
                case XDG_TOPLEVEL_STATE_TILED_LEFT:
                case XDG_TOPLEVEL_STATE_TILED_RIGHT:
                    w->pending_max = true; break;
                case XDG_TOPLEVEL_STATE_ACTIVATED:
                    w->pending_active = true; break;
            }
        }
    },
    .close = [](void* data, xdg_toplevel*) { static_cast<WaylandWindow*>(data)->emit(CloseEvent{}); },
    .configure_bounds = [](void*, xdg_toplevel*, int32_t, int32_t) {},
    .wm_capabilities = [](void*, xdg_toplevel*, wl_array*) {},
};

WaylandWindow::WaylandWindow(WaylandDisplay* disp, Application& a, const WindowDesc& desc)
    : d(disp), app(a), size_(desc.size), min_size_(desc.min_size) {
    surface = wl_compositor_create_surface(d->compositor);
    wl_surface_add_listener(surface, &surface_listener, this);
    wl_surface_set_user_data(surface, this);
    xsurface = xdg_wm_base_get_xdg_surface(d->wm_base, surface);
    xdg_surface_add_listener(xsurface, &xsurface_listener, this);
    toplevel = xdg_surface_get_toplevel(xsurface);
    xdg_toplevel_add_listener(toplevel, &toplevel_listener, this);
    xdg_toplevel_set_title(toplevel, desc.title.c_str());
    xdg_toplevel_set_app_id(toplevel, desc.app_id.c_str());
    xdg_toplevel_set_min_size(toplevel, (int)desc.min_size.width, (int)desc.min_size.height);
    if (d->deco_manager) {
        deco = zxdg_decoration_manager_v1_get_toplevel_decoration(d->deco_manager, toplevel);
        zxdg_toplevel_decoration_v1_set_mode(deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
    }
    wl_surface_commit(surface);   // no buffer: asks for the first configure
    d->windows.push_back(this);
}

void WaylandWindow::close() {
    if (!surface) return;
    if (d->pointer_focus == this) d->pointer_focus = nullptr;
    if (d->keyboard_focus == this) { d->keyboard_focus = nullptr; d->stop_repeat(); }
    if (frame_cb) { wl_callback_destroy(frame_cb); frame_cb = nullptr; }
    for (ShmBuffer& b : buffers) destroy_buffer(b);
    if (deco) zxdg_toplevel_decoration_v1_destroy(deco);
    xdg_toplevel_destroy(toplevel);
    xdg_surface_destroy(xsurface);
    wl_surface_destroy(surface);
    deco = nullptr; toplevel = nullptr; xsurface = nullptr; surface = nullptr;
    configured = false;
    wl_display_flush(d->display);
}

WaylandWindow::~WaylandWindow() {
    close();
    auto& v = d->windows;
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

// ---------------------------------------------------------------------------- cursors

const char* cursor_name(MouseCursor s) {
    switch (s) {
        case MouseCursor::Text:     return "text";
        case MouseCursor::Pointer:  return "pointer";
        case MouseCursor::Move:     return "move";
        case MouseCursor::ResizeN:  return "n-resize";
        case MouseCursor::ResizeS:  return "s-resize";
        case MouseCursor::ResizeE:  return "e-resize";
        case MouseCursor::ResizeW:  return "w-resize";
        case MouseCursor::ResizeNE: return "ne-resize";
        case MouseCursor::ResizeNW: return "nw-resize";
        case MouseCursor::ResizeSE: return "se-resize";
        case MouseCursor::ResizeSW: return "sw-resize";
        default:                    return "default";
    }
}

void WaylandDisplay::apply_cursor(int scale) {
    if (!pointer) return;
    auto& theme = cursor_themes[scale];
    if (!theme) {
        const char* name = getenv("XCURSOR_THEME");
        const char* size_s = getenv("XCURSOR_SIZE");
        int size = size_s ? atoi(size_s) : 24;
        theme = wl_cursor_theme_load(name, (size > 0 ? size : 24) * scale, shm);
        if (!theme) return;
    }
    wl_cursor* c = wl_cursor_theme_get_cursor(theme, cursor_name(cursor_shape));
    if (!c) c = wl_cursor_theme_get_cursor(theme, "left_ptr");
    if (!c || c->image_count == 0) return;
    wl_cursor_image* img = c->images[0];
    wl_pointer_set_cursor(pointer, pointer_enter_serial, cursor_surface,
                          (int32_t)img->hotspot_x / scale, (int32_t)img->hotspot_y / scale);
    wl_surface_set_buffer_scale(cursor_surface, scale);
    wl_surface_attach(cursor_surface, wl_cursor_image_get_buffer(img), 0, 0);
    wl_surface_damage_buffer(cursor_surface, 0, 0, (int32_t)img->width, (int32_t)img->height);
    wl_surface_commit(cursor_surface);
}

WaylandWindow* WaylandDisplay::window_for(wl_surface* s) {
    if (!s) return nullptr;
    for (WaylandWindow* w : windows) if (w->surface == s) return w;
    return nullptr;
}

// ---------------------------------------------------------------------------- pointer

PointerEvent make_pointer(WaylandDisplay* d, PointerEvent::Type t) {
    PointerEvent e; e.type = t; e.mods = d->mods;
    // Surface-local -> window-local (the shadow margin lies at negative/overflowing coordinates).
    Insets i = d->pointer_focus ? d->pointer_focus->insets() : Insets{};
    e.pos = {d->pointer_pos.x - i.left, d->pointer_pos.y - i.top};
    return e;
}

const wl_pointer_listener pointer_listener = {
    .enter = [](void* data, wl_pointer*, uint32_t serial, wl_surface* s, wl_fixed_t x, wl_fixed_t y) {
        auto* d = static_cast<WaylandDisplay*>(data);
        d->pointer_focus = d->window_for(s);
        d->pointer_enter_serial = serial;
        d->pointer_pos = {(float)wl_fixed_to_double(x), (float)wl_fixed_to_double(y)};
        if (!d->pointer_focus) return;
        d->cursor_shape = d->pointer_focus->cursor_;
        d->apply_cursor(d->pointer_focus->scale_);
        d->pointer_focus->emit(make_pointer(d, PointerEvent::Type::Enter));
    },
    .leave = [](void* data, wl_pointer*, uint32_t, wl_surface*) {
        auto* d = static_cast<WaylandDisplay*>(data);
        if (d->pointer_focus) d->pointer_focus->emit(make_pointer(d, PointerEvent::Type::Leave));
        d->pointer_focus = nullptr;
    },
    .motion = [](void* data, wl_pointer*, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
        auto* d = static_cast<WaylandDisplay*>(data);
        d->pointer_pos = {(float)wl_fixed_to_double(x), (float)wl_fixed_to_double(y)};
        if (!d->pointer_focus) return;
        auto e = make_pointer(d, PointerEvent::Type::Move); e.time_ms = time;
        d->pointer_focus->emit(e);
    },
    .button = [](void* data, wl_pointer*, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
        auto* d = static_cast<WaylandDisplay*>(data);
        d->last_input_serial = serial;
        if (!d->pointer_focus) return;
        PointerButton b = button == BTN_LEFT ? PointerButton::Left
                        : button == BTN_RIGHT ? PointerButton::Right
                        : button == BTN_MIDDLE ? PointerButton::Middle : PointerButton::Other;
        bool down = state == WL_POINTER_BUTTON_STATE_PRESSED;
        auto e = make_pointer(d, down ? PointerEvent::Type::Down : PointerEvent::Type::Up);
        e.button = b; e.time_ms = time;
        if (down) e.click_count = d->clicks.press(b, d->pointer_pos, time);
        d->pointer_focus->emit(e);
    },
    .axis = [](void* data, wl_pointer*, uint32_t, uint32_t axis, wl_fixed_t value) {
        auto* d = static_cast<WaylandDisplay*>(data);
        if (!d->pointer_focus) return;
        ScrollEvent e; e.mods = d->mods;
        Insets i = d->pointer_focus->insets();
        e.pos = {d->pointer_pos.x - i.left, d->pointer_pos.y - i.top};
        float v = (float)wl_fixed_to_double(value);
        if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) e.dy = v; else e.dx = v;
        d->pointer_focus->emit(e);
    },
    .frame = [](void*, wl_pointer*) {},
    .axis_source = [](void*, wl_pointer*, uint32_t) {},
    .axis_stop = [](void*, wl_pointer*, uint32_t, uint32_t) {},
    .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) {},
};

// ---------------------------------------------------------------------------- keyboard

void WaylandDisplay::stop_repeat() {
    if (repeat_timer) app->cancel_timer(repeat_timer);
    repeat_timer = 0; repeat_key = 0;
}

void WaylandDisplay::emit_key(uint32_t key, bool pressed, bool repeat) {
    if (!keyboard_focus || !xkb_state_) return;
    xkb_keycode_t code = key + 8;
    KeyEvent ke;
    ke.pressed = pressed; ke.repeat = repeat; ke.mods = mods;
    ke.keysym = xkb_state_key_get_one_sym(xkb_state_, code);
    keyboard_focus->emit(ke);
    if (!pressed) return;
    char buf[64];
    int n = xkb_state_key_get_utf8(xkb_state_, code, buf, sizeof buf);
    // Skip control characters (Ctrl+X, Escape, Backspace...): those arrive as KeyEvents.
    if (n > 0 && (unsigned char)buf[0] >= 0x20 && buf[0] != 0x7f && !(mods & (MOD_CTRL | MOD_SUPER)))
        keyboard_focus->emit(TextEvent{std::string(buf, (size_t)n)});
}

const wl_keyboard_listener keyboard_listener = {
    .keymap = [](void* data, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size) {
        auto* d = static_cast<WaylandDisplay*>(data);
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
        void* map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (map == MAP_FAILED) return;
        xkb_keymap* km = xkb_keymap_new_from_string(d->xkb_ctx, static_cast<const char*>(map),
                                                    XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map, size);
        if (!km) { fprintf(stderr, "uitk: failed to compile keymap\n"); return; }
        if (d->xkb_state_) xkb_state_unref(d->xkb_state_);
        if (d->xkb_keymap_) xkb_keymap_unref(d->xkb_keymap_);
        d->xkb_keymap_ = km;
        d->xkb_state_ = xkb_state_new(km);
    },
    .enter = [](void* data, wl_keyboard*, uint32_t, wl_surface* s, wl_array*) {
        auto* d = static_cast<WaylandDisplay*>(data);
        d->keyboard_focus = d->window_for(s);
    },
    .leave = [](void* data, wl_keyboard*, uint32_t, wl_surface*) {
        auto* d = static_cast<WaylandDisplay*>(data);
        d->stop_repeat();
        d->keyboard_focus = nullptr;
    },
    .key = [](void* data, wl_keyboard*, uint32_t serial, uint32_t, uint32_t key, uint32_t state) {
        auto* d = static_cast<WaylandDisplay*>(data);
        d->last_input_serial = serial;
        bool pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
        d->emit_key(key, pressed, false);
        if (!pressed) { if (key == d->repeat_key) d->stop_repeat(); return; }
        if (!d->xkb_keymap_ || d->repeat_rate <= 0 || !xkb_keymap_key_repeats(d->xkb_keymap_, key + 8)) return;
        d->stop_repeat();
        d->repeat_key = key;
        d->repeat_timer = d->app->add_timer(std::chrono::milliseconds(d->repeat_delay), [d, key] {
            d->repeat_timer = d->app->add_timer(std::chrono::milliseconds(1000 / d->repeat_rate),
                                                [d, key] { d->emit_key(key, true, true); }, true);
            d->emit_key(key, true, true);
        });
    },
    .modifiers = [](void* data, wl_keyboard*, uint32_t, uint32_t dep, uint32_t lat, uint32_t lock, uint32_t group) {
        auto* d = static_cast<WaylandDisplay*>(data);
        if (!d->xkb_state_) return;
        xkb_state_update_mask(d->xkb_state_, dep, lat, lock, 0, 0, group);
        auto active = [d](const char* name) {
            return xkb_state_mod_name_is_active(d->xkb_state_, name, XKB_STATE_MODS_EFFECTIVE) > 0;
        };
        d->mods = (active(XKB_MOD_NAME_SHIFT) ? MOD_SHIFT : 0) | (active(XKB_MOD_NAME_CTRL) ? MOD_CTRL : 0) |
                  (active(XKB_MOD_NAME_ALT) ? MOD_ALT : 0)     | (active(XKB_MOD_NAME_LOGO) ? MOD_SUPER : 0);
    },
    .repeat_info = [](void* data, wl_keyboard*, int32_t rate, int32_t delay) {
        auto* d = static_cast<WaylandDisplay*>(data);
        d->repeat_rate = rate; d->repeat_delay = delay;
    },
};

// ---------------------------------------------------------------------------- seat / output / registry

const wl_seat_listener seat_listener = {
    .capabilities = [](void* data, wl_seat* seat, uint32_t caps) {
        auto* d = static_cast<WaylandDisplay*>(data);
        bool has_ptr = caps & WL_SEAT_CAPABILITY_POINTER, has_kb = caps & WL_SEAT_CAPABILITY_KEYBOARD;
        if (has_ptr && !d->pointer) {
            d->pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(d->pointer, &pointer_listener, d);
        } else if (!has_ptr && d->pointer) {
            wl_pointer_release(d->pointer); d->pointer = nullptr; d->pointer_focus = nullptr;
        }
        if (has_kb && !d->keyboard) {
            d->keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
        } else if (!has_kb && d->keyboard) {
            wl_keyboard_release(d->keyboard); d->keyboard = nullptr; d->keyboard_focus = nullptr;
        }
    },
    .name = [](void*, wl_seat*, const char*) {},
};

const wl_output_listener output_listener = {
    .geometry = [](void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t) {},
    .mode = [](void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {},
    .done = [](void* data, wl_output* o) {
        auto* d = static_cast<WaylandDisplay*>(data);
        for (WaylandWindow* w : d->windows)
            if (std::find(w->entered_outputs.begin(), w->entered_outputs.end(), o) != w->entered_outputs.end())
                w->update_scale();
    },
    .scale = [](void* data, wl_output* o, int32_t factor) {
        auto* d = static_cast<WaylandDisplay*>(data);
        for (auto& out : d->outputs) if (out.output == o) out.scale = factor;
    },
    .name = [](void*, wl_output*, const char*) {},
    .description = [](void*, wl_output*, const char*) {},
};

const xdg_wm_base_listener wm_base_listener = {
    .ping = [](void*, xdg_wm_base* b, uint32_t serial) { xdg_wm_base_pong(b, serial); },
};

const wl_registry_listener registry_listener = {
    .global = [](void* data, wl_registry* reg, uint32_t name, const char* iface, uint32_t version) {
        auto* d = static_cast<WaylandDisplay*>(data);
        auto bind = [&](const wl_interface* i, uint32_t want) {
            return wl_registry_bind(reg, name, i, std::min(version, want));
        };
        if (!strcmp(iface, wl_compositor_interface.name)) {
            d->compositor_version = std::min(version, 6u);
            d->compositor = static_cast<wl_compositor*>(bind(&wl_compositor_interface, 6));
        } else if (!strcmp(iface, wl_shm_interface.name)) {
            d->shm = static_cast<wl_shm*>(bind(&wl_shm_interface, 1));
        } else if (!strcmp(iface, wl_seat_interface.name) && !d->seat) {
            d->seat = static_cast<wl_seat*>(bind(&wl_seat_interface, 5));
            wl_seat_add_listener(d->seat, &seat_listener, d);
        } else if (!strcmp(iface, wl_output_interface.name)) {
            auto* o = static_cast<wl_output*>(bind(&wl_output_interface, 4));
            wl_output_add_listener(o, &output_listener, d);
            d->outputs.push_back({o, name, 1});
        } else if (!strcmp(iface, xdg_wm_base_interface.name)) {
            d->wm_base = static_cast<xdg_wm_base*>(bind(&xdg_wm_base_interface, 5));
            xdg_wm_base_add_listener(d->wm_base, &wm_base_listener, d);
        } else if (!strcmp(iface, zxdg_decoration_manager_v1_interface.name)) {
            d->deco_manager = static_cast<zxdg_decoration_manager_v1*>(bind(&zxdg_decoration_manager_v1_interface, 1));
        }
    },
    .global_remove = [](void* data, wl_registry*, uint32_t name) {
        auto* d = static_cast<WaylandDisplay*>(data);
        auto& v = d->outputs;
        for (auto it = v.begin(); it != v.end(); ++it)
            if (it->name == name) {
                for (WaylandWindow* w : d->windows) {
                    auto& e = w->entered_outputs;
                    e.erase(std::remove(e.begin(), e.end(), it->output), e.end());
                }
                wl_output_destroy(it->output);
                v.erase(it);
                break;
            }
    },
};

// ---------------------------------------------------------------------------- display lifecycle

WaylandDisplay* WaylandDisplay::get(Application& app) {
    static WaylandDisplay* instance = nullptr;
    static bool tried = false;
    if (tried) return instance;
    tried = true;

    wl_display* dpy = wl_display_connect(nullptr);
    if (!dpy) return nullptr;
    auto* d = new WaylandDisplay;
    d->display = dpy;
    d->app = &app;
    d->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    d->registry = wl_display_get_registry(dpy);
    wl_registry_add_listener(d->registry, &registry_listener, d);
    wl_display_roundtrip(dpy);   // globals
    wl_display_roundtrip(dpy);   // seat capabilities, output scales
    if (!d->compositor || !d->shm || !d->wm_base) {
        fprintf(stderr, "uitk: Wayland compositor lacks wl_compositor/wl_shm/xdg_wm_base\n");
        wl_display_disconnect(dpy);
        delete d;
        return nullptr;
    }
    d->cursor_surface = wl_compositor_create_surface(d->compositor);
    app.add_source(d);
    instance = d;
    return d;
}

void WaylandDisplay::prepare() {
    while (wl_display_prepare_read(display) != 0) wl_display_dispatch_pending(display);
    if (wl_display_flush(display) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(display);
        fprintf(stderr, "uitk: lost connection to the Wayland compositor\n");
        app->quit(1);
        prepared = false;
        return;
    }
    prepared = true;
}

void WaylandDisplay::dispatch(short revents) {
    if (!prepared) return;
    prepared = false;
    if (revents & POLLIN) wl_display_read_events(display);
    else wl_display_cancel_read(display);
    if (revents & (POLLERR | POLLHUP)) {
        fprintf(stderr, "uitk: Wayland connection closed\n");
        app->quit(1);
        return;
    }
    wl_display_dispatch_pending(display);
}

} // namespace

std::unique_ptr<Window> create_wayland_window(Application& app, const WindowDesc& desc) {
    WaylandDisplay* d = WaylandDisplay::get(app);
    if (!d) return nullptr;
    return std::make_unique<WaylandWindow>(d, app, desc);
}

} // namespace uitk::linux_impl
