// X11 backend: undecorated window, our own chrome.
// - 32-bit ARGB visual for transparent rounded corners under a compositor.
// - Move/resize via _NET_WM_MOVERESIZE, maximize via _NET_WM_STATE, window menu via _GTK_SHOW_WINDOW_MENU.
// - HiDPI from Xft.dpi (fractional scale allowed), XIM for UTF-8 text input.
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/sync.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

#include "uitk/core/application.h"
#include "../linux_common.h"
#include "../../common/frame_pacer.h"

namespace uitk::linux_impl {
namespace {

class X11Window;

// EWMH _NET_WM_MOVERESIZE directions
enum { MR_TOPLEFT = 0, MR_TOP = 1, MR_TOPRIGHT = 2, MR_RIGHT = 3, MR_BOTTOMRIGHT = 4,
       MR_BOTTOM = 5, MR_BOTTOMLEFT = 6, MR_LEFT = 7, MR_MOVE = 8 };

int moveresize_dir(unsigned e) {
    switch (e) {
        case EDGE_TOP | EDGE_LEFT:     return MR_TOPLEFT;
        case EDGE_TOP | EDGE_RIGHT:    return MR_TOPRIGHT;
        case EDGE_BOTTOM | EDGE_LEFT:  return MR_BOTTOMLEFT;
        case EDGE_BOTTOM | EDGE_RIGHT: return MR_BOTTOMRIGHT;
        case EDGE_TOP:    return MR_TOP;
        case EDGE_BOTTOM: return MR_BOTTOM;
        case EDGE_LEFT:   return MR_LEFT;
        case EDGE_RIGHT:  return MR_RIGHT;
    }
    return MR_MOVE;
}

// ============================================================================ display

class X11Display final : public EventSource {
public:
    static X11Display* get(Application& app);

    Handle handle() const override { return ConnectionNumber(dpy); }
    void prepare() override { process_queued(); XFlush(dpy); }
    void dispatch(short) override { process_queued(); }

    void process_queued();
    void handle(XEvent& ev);
    X11Window* find(::Window w);

    Display* dpy = nullptr;
    int      scr = 0;
    ::Window root = 0;
    Visual*  visual = nullptr;
    int      depth = 24;
    Colormap colormap = 0;
    float    scale = 1.f;
    XIM      xim = nullptr;
    bool     has_shm = false;          // MIT-SHM: present without copying through the socket
    int      shm_completion = -1;      // event type of XShmCompletionEvent
    Application* app = nullptr;
    std::vector<X11Window*> windows;
    std::map<MouseCursor, Cursor> cursors;

    Atom gtk_frame_extents, net_wm_sync_request, net_wm_sync_request_counter;
    bool has_sync = false;            // XSync extension (for _NET_WM_SYNC_REQUEST)
    Atom wm_protocols, wm_delete, net_wm_state, net_max_v, net_max_h, net_moveresize,
         motif_hints, net_wm_name, utf8_string, gtk_show_window_menu;

    // last button press (root coords) for move/resize requests
    int press_x_root = 0, press_y_root = 0;
    unsigned press_button = Button1;
    ClickTracker clicks;
    unsigned last_keycode = 0;

    Cursor cursor(MouseCursor s);
};

// ============================================================================ window

class X11Window final : public Window {
public:
    X11Window(X11Display* d, Application& app, const WindowDesc& desc);
    ~X11Window() override;

    const char* platform_name() const override { return "x11"; }
    // pw_/ph_ is the whole X window (incl. shadow margin); size() is the visible window.
    Size  size() const override {
        Insets i = insets();
        return {pw_ / d->scale - i.left - i.right, ph_ / d->scale - i.top - i.bottom};
    }
    Insets insets() const override { return maximized_ ? Insets{} : requested_insets_; }
    void set_shadow_insets(Insets i, float grip) override;
    void apply_frame_extents();
    float scale() const override { return d->scale; }
    bool  maximized() const override { return maximized_; }
    bool  active() const override { return active_; }

    void set_title(const std::string& t) override;
    void minimize() override { XIconifyWindow(d->dpy, win, d->scr); XFlush(d->dpy); }
    void set_maximized(bool m) override;
    void begin_move() override { moveresize(MR_MOVE); }
    void begin_resize(unsigned edges) override { moveresize(moveresize_dir(edges)); }
    void show_window_menu(Point p) override;
    void set_cursor(MouseCursor s) override {
        if (s == cursor_) return;
        cursor_ = s;
        XDefineCursor(d->dpy, win, d->cursor(s));
    }
    void request_redraw() override;
    void close() override;

    NativeSurface native_surface() const override {
        NativeSurface ns{NativeSurface::Kind::X11};
        ns.display = d->dpy; ns.window = win; ns.visual_id = XVisualIDFromVisual(d->visual);
        return ns;
    }
    SoftwareBuffer acquire_software_buffer() override;
    void present_software_buffer() override;
    void will_present_gpu() override { presented_ = true; }

    void emit(const Event& e) { if (on_event) on_event(e); }
    void moveresize(int dir);
    void paint_now();
    bool query_maximized();

    X11Display* d;
    Application& app;
    ::Window win = 0;
    GC       gc = nullptr;
    XIC      xic = nullptr;
    // Software frame buffers. With MIT-SHM two segments alternate, so drawing the next frame
    // never waits for the server to finish copying the previous one.
    struct FrameImage {
        XImage* image = nullptr;
        std::vector<uint32_t> pixels;  // backing store when not using MIT-SHM
        XShmSegmentInfo shm{};
        bool shm_image = false;
        bool busy = false;             // server still reading this segment
    };
    FrameImage frames_[2];
    FrameImage* current_ = nullptr;
    void destroy_image(FrameImage& f);
    bool create_shm_image(FrameImage& f);
    int  pw_ = 0, ph_ = 0;          // physical size of the X window (incl. margins)
    Insets requested_insets_;
    float grip_ = 0;
    Size  min_size_;
    bool maximized_ = false, active_ = false, mapped_ = false;
    MouseCursor cursor_ = MouseCursor::Default;

    bool dirty = false;
    platform::FramePacer pacer{app, [this] { paint_now(); }};
    // _NET_WM_SYNC_REQUEST: the WM waits for us to draw each new size before showing it,
    // so interactive resizes never reveal an undrawn window.
    XSyncCounter sync_counter = None;
    XSyncValue   sync_value{};
    bool         sync_pending = false;
    bool         presented_ = false;   // a frame was presented during the current paint
    std::chrono::steady_clock::time_point last_configure{}; // last size change (resize in progress?)
};

// ---------------------------------------------------------------------------- display setup

float read_xft_scale(Display* dpy) {
    float scale = 1.f;
    if (const char* env = getenv("UITK_SCALE")) { float s = (float)atof(env); if (s > 0) return s; }
    char* rms = XResourceManagerString(dpy);
    if (!rms) return scale;
    XrmInitialize();
    XrmDatabase db = XrmGetStringDatabase(rms);
    if (!db) return scale;
    char* type = nullptr; XrmValue v;
    if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &v) && v.addr) {
        float dpi = (float)atof(v.addr);
        if (dpi > 0) scale = dpi / 96.f;
    }
    XrmDestroyDatabase(db);
    return scale;
}

X11Display* X11Display::get(Application& app) {
    static X11Display* instance = nullptr;
    static bool tried = false;
    if (tried) return instance;
    tried = true;

    setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) { fprintf(stderr, "uitk: cannot open X display\n"); return nullptr; }

    auto* d = new X11Display;
    d->dpy = dpy;
    d->app = &app;
    d->scr = DefaultScreen(dpy);
    d->root = RootWindow(dpy, d->scr);
    d->scale = read_xft_scale(dpy);

    XVisualInfo vi{};
    if (XMatchVisualInfo(dpy, d->scr, 32, TrueColor, &vi)) {
        d->visual = vi.visual; d->depth = 32;
    } else {
        d->visual = DefaultVisual(dpy, d->scr); d->depth = DefaultDepth(dpy, d->scr);
        fprintf(stderr, "uitk: no 32-bit visual; window corners will be opaque\n");
    }
    d->colormap = XCreateColormap(dpy, d->root, d->visual, AllocNone);

    d->wm_protocols   = XInternAtom(dpy, "WM_PROTOCOLS", False);
    d->wm_delete      = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    d->net_wm_state   = XInternAtom(dpy, "_NET_WM_STATE", False);
    d->net_max_v      = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    d->net_max_h      = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    d->net_moveresize = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
    d->motif_hints    = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    d->net_wm_name    = XInternAtom(dpy, "_NET_WM_NAME", False);
    d->utf8_string    = XInternAtom(dpy, "UTF8_STRING", False);
    d->gtk_show_window_menu = XInternAtom(dpy, "_GTK_SHOW_WINDOW_MENU", False);
    d->gtk_frame_extents = XInternAtom(dpy, "_GTK_FRAME_EXTENTS", False);
    d->net_wm_sync_request = XInternAtom(dpy, "_NET_WM_SYNC_REQUEST", False);
    d->net_wm_sync_request_counter = XInternAtom(dpy, "_NET_WM_SYNC_REQUEST_COUNTER", False);
    {
        int ev_base, err_base, major, minor;
        d->has_sync = XSyncQueryExtension(dpy, &ev_base, &err_base) && XSyncInitialize(dpy, &major, &minor);
    }

    Bool supported = False;
    XkbSetDetectableAutoRepeat(dpy, True, &supported);   // autorepeat sends presses only
    d->xim = XOpenIM(dpy, nullptr, nullptr, nullptr);
    if (XShmQueryExtension(dpy) && !getenv("UITK_NO_SHM")) {
        d->has_shm = true;
        d->shm_completion = XShmGetEventBase(dpy) + ShmCompletion;
    }

    app.add_source(d);
    instance = d;
    return d;
}

Cursor X11Display::cursor(MouseCursor s) {
    auto it = cursors.find(s);
    if (it != cursors.end()) return it->second;
    unsigned shape = XC_left_ptr;
    switch (s) {
        case MouseCursor::Text:     shape = XC_xterm; break;
        case MouseCursor::Pointer:  shape = XC_hand2; break;
        case MouseCursor::Move:     shape = XC_fleur; break;
        case MouseCursor::ResizeN:  shape = XC_top_side; break;
        case MouseCursor::ResizeS:  shape = XC_bottom_side; break;
        case MouseCursor::ResizeE:  shape = XC_right_side; break;
        case MouseCursor::ResizeW:  shape = XC_left_side; break;
        case MouseCursor::ResizeNE: shape = XC_top_right_corner; break;
        case MouseCursor::ResizeNW: shape = XC_top_left_corner; break;
        case MouseCursor::ResizeSE: shape = XC_bottom_right_corner; break;
        case MouseCursor::ResizeSW: shape = XC_bottom_left_corner; break;
        default: break;
    }
    return cursors[s] = XCreateFontCursor(dpy, shape);
}

X11Window* X11Display::find(::Window w) {
    for (X11Window* x : windows) if (x->win == w) return x;
    return nullptr;
}

void X11Display::process_queued() {
    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (XFilterEvent(&ev, None)) continue;   // consumed by the input method
        handle(ev);
    }
}

uint32_t x_mods(unsigned state) {
    return (state & ShiftMask ? mod::Shift : 0) | (state & ControlMask ? mod::Ctrl : 0) |
           (state & Mod1Mask ? mod::Alt : 0) | (state & Mod4Mask ? mod::Super : 0);
}

void X11Display::handle(XEvent& ev) {
    X11Window* w = find(ev.xany.window);
    if (!w) return;
    const float s = scale;
    switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0) w->request_redraw();
            break;
        case MapNotify:
            w->mapped_ = true; w->request_redraw();
            break;
        case UnmapNotify:
            w->mapped_ = false;
            break;
        case ConfigureNotify:
            if (ev.xconfigure.width != w->pw_ || ev.xconfigure.height != w->ph_) {
                w->pw_ = ev.xconfigure.width; w->ph_ = ev.xconfigure.height;
                w->last_configure = std::chrono::steady_clock::now();
                w->apply_frame_extents();   // input shape follows the size
                w->emit(ResizeEvent{w->size(), s});
                w->request_redraw();
            }
            break;
        case PropertyNotify:
            if (ev.xproperty.atom == net_wm_state) {
                bool m = w->query_maximized();
                if (m != w->maximized_) {
                    w->maximized_ = m;
                    w->apply_frame_extents();   // no shadow margin while maximized
                    w->emit(StateEvent{m});
                    w->emit(ResizeEvent{w->size(), s});
                    w->request_redraw();
                }
            }
            break;
        case FocusIn:
            if (w->xic) XSetICFocus(w->xic);
            if (!w->active_) { w->active_ = true; w->emit(FocusEvent{true}); }
            break;
        case FocusOut:
            if (w->xic) XUnsetICFocus(w->xic);
            if (w->active_) { w->active_ = false; w->emit(FocusEvent{false}); }
            break;
        case EnterNotify: case LeaveNotify: case MotionNotify: {
            PointerEvent e;
            e.type = ev.type == EnterNotify ? PointerEvent::Type::Enter
                   : ev.type == LeaveNotify ? PointerEvent::Type::Leave : PointerEvent::Type::Move;
            Insets in = w->insets();
            e.pos = {ev.xmotion.x / s - in.left, ev.xmotion.y / s - in.top};
            e.mods = x_mods(ev.xmotion.state);
            e.time_ms = (uint32_t)ev.xmotion.time;
            w->emit(e);
            break;
        }
        case ButtonPress: case ButtonRelease: {
            unsigned b = ev.xbutton.button;
            Insets in = w->insets();
            Point pos{ev.xbutton.x / s - in.left, ev.xbutton.y / s - in.top};
            if (b >= 4 && b <= 7) {           // wheel
                if (ev.type == ButtonRelease) break;
                ScrollEvent se; se.pos = pos; se.mods = x_mods(ev.xbutton.state);
                const float step = 15.f;
                if (b == 4) se.dy = -step; else if (b == 5) se.dy = step;
                else if (b == 6) se.dx = -step; else se.dx = step;
                w->emit(se);
                break;
            }
            PointerEvent e;
            e.type = ev.type == ButtonPress ? PointerEvent::Type::Down : PointerEvent::Type::Up;
            e.pos = pos;
            e.button = b == Button1 ? PointerButton::Left : b == Button3 ? PointerButton::Right
                     : b == Button2 ? PointerButton::Middle : PointerButton::Other;
            e.mods = x_mods(ev.xbutton.state);
            e.time_ms = (uint32_t)ev.xbutton.time;
            if (ev.type == ButtonPress) {
                press_x_root = ev.xbutton.x_root; press_y_root = ev.xbutton.y_root; press_button = b;
                e.click_count = clicks.press(e.button, pos, e.time_ms);
            }
            w->emit(e);
            break;
        }
        case KeyPress: {
            char buf[64] = {};
            KeySym sym = NoSymbol;
            int n = 0;
            if (w->xic) {
                Status st;
                n = Xutf8LookupString(w->xic, &ev.xkey, buf, sizeof buf - 1, &sym, &st);
                if (st == XBufferOverflow) n = 0;
            } else {
                n = XLookupString(&ev.xkey, buf, sizeof buf - 1, &sym, nullptr);
            }
            KeyEvent ke;
            ke.pressed = true;
            ke.repeat = ev.xkey.keycode == last_keycode;
            ke.keysym = (uint32_t)sym;
            ke.mods = x_mods(ev.xkey.state);
            last_keycode = ev.xkey.keycode;
            w->emit(ke);
            if (n > 0 && (unsigned char)buf[0] >= 0x20 && buf[0] != 0x7f && !(ke.mods & (mod::Ctrl | mod::Super)))
                w->emit(TextEvent{std::string(buf, (size_t)n)});
            break;
        }
        case KeyRelease: {
            KeySym sym = NoSymbol;
            char buf[8];
            XLookupString(&ev.xkey, buf, sizeof buf, &sym, nullptr);
            if (ev.xkey.keycode == last_keycode) last_keycode = 0;
            KeyEvent ke; ke.pressed = false; ke.keysym = (uint32_t)sym; ke.mods = x_mods(ev.xkey.state);
            w->emit(ke);
            break;
        }
        default:
            if (ev.type == shm_completion) {   // XShmCompletionEvent: a frame was copied
                auto& ce = reinterpret_cast<XShmCompletionEvent&>(ev);
                for (auto& f : w->frames_)
                    if (f.shm_image && f.shm.shmseg == ce.shmseg) f.busy = false;
                if (w->dirty) w->request_redraw();
            }
            break;
        case ClientMessage:
            if (ev.xclient.message_type == wm_protocols && (Atom)ev.xclient.data.l[0] == wm_delete) {
                w->emit(CloseEvent{});
            } else if (ev.xclient.message_type == wm_protocols &&
                       (Atom)ev.xclient.data.l[0] == net_wm_sync_request && w->sync_counter != None) {
                // Remember the serial; it is acknowledged after the next frame is presented.
                XSyncIntsToValue(&w->sync_value, (unsigned)ev.xclient.data.l[2], (int)ev.xclient.data.l[3]);
                w->sync_pending = true;
            }
            break;
    }
}

// ---------------------------------------------------------------------------- window

X11Window::X11Window(X11Display* disp, Application& a, const WindowDesc& desc) : d(disp), app(a) {
    Display* dpy = d->dpy;
    min_size_ = desc.min_size;
    pw_ = (int)std::lround(desc.size.width * d->scale);
    ph_ = (int)std::lround(desc.size.height * d->scale);

    XSetWindowAttributes swa{};
    swa.colormap = d->colormap;
    swa.border_pixel = 0;
    // No background: the server must never clear the window (that shows as a transparent blink
    // while resizing). NorthWest bit gravity keeps the old pixels until the new frame lands.
    swa.background_pixmap = None;
    swa.bit_gravity = NorthWestGravity;
    swa.event_mask = ExposureMask | StructureNotifyMask | PropertyChangeMask | FocusChangeMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask |
                     LeaveWindowMask | KeyPressMask | KeyReleaseMask;
    win = XCreateWindow(dpy, d->root, 100, 100, pw_, ph_, 0, d->depth, InputOutput, d->visual,
                        CWColormap | CWBorderPixel | CWBackPixmap | CWBitGravity | CWEventMask, &swa);
    gc = XCreateGC(dpy, win, 0, nullptr);

    XClassHint cls{const_cast<char*>(desc.app_id.c_str()), const_cast<char*>(desc.app_id.c_str())};
    XSetClassHint(dpy, win, &cls);
    set_title(desc.title);

    XSizeHints sh{};
    sh.flags = PMinSize;
    sh.min_width = (int)std::lround(desc.min_size.width * d->scale);
    sh.min_height = (int)std::lround(desc.min_size.height * d->scale);
    XSetWMNormalHints(dpy, win, &sh);
    Atom protocols[2] = {d->wm_delete, d->net_wm_sync_request};
    if (d->has_sync) {
        XSyncValue zero;
        XSyncIntToValue(&zero, 0);
        sync_counter = XSyncCreateCounter(dpy, zero);
        unsigned long counter = sync_counter;
        XChangeProperty(dpy, win, d->net_wm_sync_request_counter, XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&counter), 1);
    }
    XSetWMProtocols(dpy, win, protocols, d->has_sync ? 2 : 1);

    Atom wtype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom wtype_normal = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    XChangeProperty(dpy, win, wtype, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&wtype_normal), 1);
    long motif[5] = {2, 0, 0, 0, 0};   // MWM_HINTS_DECORATIONS, no decorations
    XChangeProperty(dpy, win, d->motif_hints, d->motif_hints, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(motif), 5);

    if (d->xim)
        xic = XCreateIC(d->xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                        XNClientWindow, win, XNFocusWindow, win, nullptr);

    XDefineCursor(dpy, win, d->cursor(MouseCursor::Default));
    d->windows.push_back(this);
    // Map on the next loop iteration: whoever created the window (e.g. WindowChrome) has then
    // set the shadow margin, so the WM sees the final _GTK_FRAME_EXTENTS and size up front
    // instead of racing a resize of an already-mapped window.
    app.post([this] { if (win) { XMapWindow(d->dpy, win); XFlush(d->dpy); } });
}

X11Window::~X11Window() {
    close();
    auto& v = d->windows;
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

void X11Window::close() {
    if (!win) return;
    for (auto& f : frames_) destroy_image(f);
    if (xic) { XDestroyIC(xic); xic = nullptr; }
    XFreeGC(d->dpy, gc);
    if (sync_counter != None) { XSyncDestroyCounter(d->dpy, sync_counter); sync_counter = None; }
    XDestroyWindow(d->dpy, win);
    XFlush(d->dpy);
    win = 0;
}

void X11Window::set_title(const std::string& t) {
    XStoreName(d->dpy, win, t.c_str());
    XChangeProperty(d->dpy, win, d->net_wm_name, d->utf8_string, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(t.data()), (int)t.size());
}

void X11Window::moveresize(int dir) {
    XUngrabPointer(d->dpy, CurrentTime);
    XEvent ev{};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = d->net_moveresize;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d->press_x_root;
    ev.xclient.data.l[1] = d->press_y_root;
    ev.xclient.data.l[2] = dir;
    ev.xclient.data.l[3] = d->press_button;
    ev.xclient.data.l[4] = 1;
    XSendEvent(d->dpy, d->root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(d->dpy);
}

void X11Window::set_maximized(bool m) {
    XEvent ev{};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = d->net_wm_state;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = m ? 1 : 0;   // _NET_WM_STATE_ADD / _REMOVE
    ev.xclient.data.l[1] = (long)d->net_max_v;
    ev.xclient.data.l[2] = (long)d->net_max_h;
    ev.xclient.data.l[3] = 1;
    XSendEvent(d->dpy, d->root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(d->dpy);
}

void X11Window::show_window_menu(Point) {
    XUngrabPointer(d->dpy, CurrentTime);
    XEvent ev{};
    ev.xclient.type = ClientMessage;
    ev.xclient.window = win;
    ev.xclient.message_type = d->gtk_show_window_menu;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 0;   // device id (core pointer)
    ev.xclient.data.l[1] = d->press_x_root;
    ev.xclient.data.l[2] = d->press_y_root;
    XSendEvent(d->dpy, d->root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(d->dpy);
}

bool X11Window::query_maximized() {
    Atom type; int fmt; unsigned long n, after; unsigned char* data = nullptr;
    bool v = false, h = false;
    if (XGetWindowProperty(d->dpy, win, d->net_wm_state, 0, 64, False, XA_ATOM,
                           &type, &fmt, &n, &after, &data) == Success && data) {
        Atom* atoms = reinterpret_cast<Atom*>(data);
        for (unsigned long i = 0; i < n; ++i) {
            if (atoms[i] == d->net_max_v) v = true;
            if (atoms[i] == d->net_max_h) h = true;
        }
        XFree(data);
    }
    return v && h;
}

void X11Window::set_shadow_insets(Insets i, float grip) {
    Size content = size();
    requested_insets_ = i;
    grip_ = grip;
    Insets e = insets();
    // Grow the X window so the visible window keeps its size; margins go around it.
    int w = (int)std::lround((content.width + e.left + e.right) * d->scale);
    int h = (int)std::lround((content.height + e.top + e.bottom) * d->scale);
    if (w != pw_ || h != ph_) { pw_ = w; ph_ = h; XResizeWindow(d->dpy, win, (unsigned)w, (unsigned)h); }

    XSizeHints sh{};
    sh.flags = PMinSize;
    sh.min_width  = (int)std::lround((min_size_.width + i.left + i.right) * d->scale);
    sh.min_height = (int)std::lround((min_size_.height + i.top + i.bottom) * d->scale);
    XSetWMNormalHints(d->dpy, win, &sh);
    apply_frame_extents();
    request_redraw();
}

void X11Window::apply_frame_extents() {
    if (!win) return;
    const float s = d->scale;
    Insets in = insets();
    // _GTK_FRAME_EXTENTS (left, right, top, bottom): the WM places, snaps and maximizes the
    // window by its inner rectangle, treating the margin as client-drawn shadow.
    long ext[4] = {std::lround(in.left * s), std::lround(in.right * s), std::lround(in.top * s), std::lround(in.bottom * s)};
    XChangeProperty(d->dpy, win, d->gtk_frame_extents, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(ext), 4);
    // Input shape: the window plus the resize grip; clicks on the rest of the shadow fall through.
    if (in.empty()) {
        XShapeCombineMask(d->dpy, win, ShapeInput, 0, 0, None, ShapeSet);
    } else {
        Size sz = size();
        float g = grip_;
        int x0 = (int)std::lround(std::max(0.f, in.left - g) * s);
        int y0 = (int)std::lround(std::max(0.f, in.top - g) * s);
        int x1 = std::min(pw_, (int)std::lround((in.left + sz.width + g) * s));
        int y1 = std::min(ph_, (int)std::lround((in.top + sz.height + g) * s));
        XRectangle r{(short)x0, (short)y0, (unsigned short)std::max(0, x1 - x0), (unsigned short)std::max(0, y1 - y0)};
        XShapeCombineRectangles(d->dpy, win, ShapeInput, 0, 0, &r, 1, ShapeSet, Unsorted);
    }
    XFlush(d->dpy);
}

void X11Window::request_redraw() {
    dirty = true;
    // No compositor frame callbacks on X11: frames are paced on a fixed 60 Hz grid.
    if (win) pacer.request();
}

void X11Window::paint_now() {
    if (!win || !mapped_) return;
    // Take in queued events first (notably ConfigureNotify during a resize drag), so the frame
    // is drawn at the window's current size rather than one that is already stale.
    // While a resize is in progress, round-trip first: every ConfigureNotify the server has
    // generated is then guaranteed to be queued, so the frame matches the window's real size
    // (otherwise a GPU frame would be skipped and the window left undrawn).
    if (std::chrono::steady_clock::now() - last_configure < std::chrono::milliseconds(300))
        XSync(d->dpy, False);
    d->process_queued();
    if (!dirty || !win) return;
    dirty = false;
    pacer.frame_drawn();
    presented_ = false;
    run_frame();   // sets the frame clock, calls on_frame, keeps animations going
    if (presented_ && sync_pending) {
        // The frame for the requested size is out: let the WM show it.
        XSyncSetCounter(d->dpy, sync_counter, sync_value);
        XFlush(d->dpy);
        sync_pending = false;
    }
    if (app.loop_mode() == LoopMode::Continuous) request_redraw();
}

void X11Window::destroy_image(FrameImage& f) {
    if (!f.image) return;
    if (f.shm_image) {
        XShmDetach(d->dpy, &f.shm);
        XSync(d->dpy, False);
        f.image->data = nullptr;
        XDestroyImage(f.image);
        shmdt(f.shm.shmaddr);
    } else {
        f.image->data = nullptr;
        XDestroyImage(f.image);
    }
    f = FrameImage{};
}

namespace {
bool g_shm_error = false;
int shm_error_handler(Display*, XErrorEvent*) { g_shm_error = true; return 0; }
}

bool X11Window::create_shm_image(FrameImage& f) {
    XImage* img = XShmCreateImage(d->dpy, d->visual, (unsigned)d->depth, ZPixmap, nullptr, &f.shm,
                                  (unsigned)pw_, (unsigned)ph_);
    if (!img) return false;
    f.shm.shmid = shmget(IPC_PRIVATE, (size_t)img->bytes_per_line * img->height, IPC_CREAT | 0600);
    if (f.shm.shmid < 0) { XDestroyImage(img); f.shm = {}; return false; }
    f.shm.shmaddr = img->data = static_cast<char*>(shmat(f.shm.shmid, nullptr, 0));
    f.shm.readOnly = False;
    // Attach errors arrive asynchronously (e.g. remote X server): trap them around a sync.
    g_shm_error = false;
    auto old = XSetErrorHandler(shm_error_handler);
    XShmAttach(d->dpy, &f.shm);
    XSync(d->dpy, False);
    XSetErrorHandler(old);
    shmctl(f.shm.shmid, IPC_RMID, nullptr);   // freed automatically once both sides detach
    if (g_shm_error || f.shm.shmaddr == reinterpret_cast<char*>(-1)) {
        if (f.shm.shmaddr != reinterpret_cast<char*>(-1)) shmdt(f.shm.shmaddr);
        img->data = nullptr;
        XDestroyImage(img);
        f.shm = {};
        return false;
    }
    f.image = img;
    f.shm_image = true;
    return true;
}

SoftwareBuffer X11Window::acquire_software_buffer() {
    current_ = nullptr;
    // MIT-SHM: use whichever segment the server is not reading. Without SHM, XPutImage copies
    // synchronously, so a single buffer suffices.
    int count = d->has_shm ? 2 : 1;
    for (int i = 0; i < count; ++i) {
        FrameImage& f = frames_[i];
        if (f.busy) continue;
        if (!f.image || f.image->width != pw_ || f.image->height != ph_) {
            destroy_image(f);
            if (!(d->has_shm && create_shm_image(f))) {
                if (d->has_shm) {
                    d->has_shm = false;
                    fprintf(stderr, "uitk: MIT-SHM unavailable, using XPutImage\n");
                    for (auto& other : frames_) destroy_image(other);
                }
                f.pixels.assign((size_t)pw_ * ph_, 0);
                f.image = XCreateImage(d->dpy, d->visual, (unsigned)d->depth, ZPixmap, 0,
                                       reinterpret_cast<char*>(f.pixels.data()), (unsigned)pw_, (unsigned)ph_, 32, pw_ * 4);
            }
        }
        current_ = &f;
        return {reinterpret_cast<uint32_t*>(f.image->data), pw_, ph_, f.image->bytes_per_line};
    }
    dirty = true;   // both segments in flight (rare): redraw on the next completion event
    return {};
}

void X11Window::present_software_buffer() {
    if (!current_ || !current_->image) return;
    FrameImage& f = *current_;
    if (f.shm_image) {
        XShmPutImage(d->dpy, win, gc, f.image, 0, 0, 0, 0, (unsigned)pw_, (unsigned)ph_, True);
        f.busy = true;
        presented_ = true;
    } else {
        XPutImage(d->dpy, win, gc, f.image, 0, 0, 0, 0, (unsigned)pw_, (unsigned)ph_);
        presented_ = true;
    }
    XFlush(d->dpy);
    current_ = nullptr;
}

} // namespace

std::unique_ptr<Window> create_x11_window(Application& app, const WindowDesc& desc) {
    X11Display* d = X11Display::get(app);
    if (!d) return nullptr;
    return std::make_unique<X11Window>(d, app, desc);
}

} // namespace uitk::linux_impl
