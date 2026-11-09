// Win32 backend: a frameless top-level window with our own title bar.
//
// - The non-client area is removed (WM_NCCALCSIZE), but the window keeps WS_THICKFRAME and
//   WS_CAPTION, so DWM still gives it the system shadow, Windows 11 rounded corners, Aero Snap
//   and the minimize/maximize animations. The client therefore paints an opaque, square frame
//   (Window::native_shadow()).
// - Move/resize hand over to the system's own modal loops (WM_NCLBUTTONDOWN), so snapping
//   works. While such a loop (or the window menu) runs, a timer keeps the app's timers, posted
//   tasks and animations going, and every size change is painted synchronously (no stretched
//   or blank frames while resizing).
// - Per-monitor DPI aware (v2); scale = DPI / 96, overridable with UITK_SCALE.
// - Keys arrive as X11 keysyms (uitk::key), text as UTF-8 TextEvents.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "uitk/core/application.h"
#include "uitk/platform/window.h"
#include "../common/click_tracker.h"
#include "../common/frame_pacer.h"
#include "win32_util.h"

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_COLOR_NONE
#define DWMWA_COLOR_NONE 0xFFFFFFFE
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace uitk::win32 {
namespace {

constexpr wchar_t kClassName[] = L"uitk.window";
constexpr UINT_PTR kPumpTimer = 1;   // keeps the app running inside system modal loops

// ---------------------------------------------------------------------------- DPI (loaded lazily)

using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
using SetDpiCtxFn = BOOL(WINAPI*)(HANDLE);
using GetMetricsForDpiFn = int(WINAPI*)(int, UINT);

HMODULE user32() { static HMODULE m = GetModuleHandleW(L"user32.dll"); return m; }

template <class Fn> Fn user32_fn(const char* name) {
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(user32(), name)));
}

void init_dpi_awareness() {
    static bool done = false;
    if (done) return;
    done = true;
    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4 (Windows 10 1703+).
    if (auto set_ctx = user32_fn<SetDpiCtxFn>("SetProcessDpiAwarenessContext"))
        if (set_ctx(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4)))) return;
    SetProcessDPIAware();   // fallback: system DPI aware
}

UINT window_dpi(HWND h) {
    static auto fn = user32_fn<GetDpiForWindowFn>("GetDpiForWindow");
    if (fn && h) if (UINT d = fn(h)) return d;
    HDC dc = GetDC(nullptr);
    UINT d = (UINT)GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return d ? d : 96;
}

float env_scale() {
    if (const char* e = getenv("UITK_SCALE")) { float s = (float)atof(e); if (s > 0) return s; }
    return 0;
}

// ---------------------------------------------------------------------------- keys

uint32_t current_mods() {
    auto down = [](int vk) { return (GetKeyState(vk) & 0x8000) != 0; };
    return (down(VK_SHIFT) ? mod::Shift : 0) | (down(VK_CONTROL) ? mod::Ctrl : 0) |
           (down(VK_MENU) ? mod::Alt : 0) | (down(VK_LWIN) || down(VK_RWIN) ? mod::Super : 0);
}

// Virtual key -> X11 keysym (the space uitk::key uses on every platform).
uint32_t keysym_for(WPARAM vk, LPARAM lp) {
    bool extended = (lp & (1 << 24)) != 0;
    UINT scan = (UINT)((lp >> 16) & 0xff);
    if (vk >= 'A' && vk <= 'Z') return (uint32_t)(vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9') return (uint32_t)vk;
    if (vk >= VK_F1 && vk <= VK_F24) return 0xffbe + (uint32_t)(vk - VK_F1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return 0xffb0 + (uint32_t)(vk - VK_NUMPAD0);
    switch (vk) {
        case VK_SPACE:   return 0x20;
        case VK_BACK:    return 0xff08;
        case VK_TAB:     return 0xff09;
        case VK_RETURN:  return extended ? 0xff8d : 0xff0d;   // KP_Enter / Return
        case VK_ESCAPE:  return 0xff1b;
        case VK_DELETE:  return 0xffff;
        case VK_INSERT:  return 0xff63;
        case VK_HOME:    return 0xff50;
        case VK_LEFT:    return 0xff51;
        case VK_UP:      return 0xff52;
        case VK_RIGHT:   return 0xff53;
        case VK_DOWN:    return 0xff54;
        case VK_PRIOR:   return 0xff55;
        case VK_NEXT:    return 0xff56;
        case VK_END:     return 0xff57;
        case VK_APPS:    return 0xff67;   // Menu
        case VK_PAUSE:   return 0xff13;
        case VK_SCROLL:  return 0xff14;
        case VK_SNAPSHOT:return 0xff61;
        case VK_CAPITAL: return 0xffe5;
        case VK_NUMLOCK: return 0xff7f;
        case VK_MULTIPLY:return 0xffaa;
        case VK_ADD:     return 0xffab;
        case VK_SEPARATOR:return 0xffac;
        case VK_SUBTRACT:return 0xffad;
        case VK_DECIMAL: return 0xffae;
        case VK_DIVIDE:  return 0xffaf;
        case VK_SHIFT:   return scan == 0x36 ? 0xffe2 : 0xffe1;
        case VK_CONTROL: return extended ? 0xffe4 : 0xffe3;
        case VK_MENU:    return extended ? 0xffea : 0xffe9;
        case VK_LWIN:    return 0xffeb;
        case VK_RWIN:    return 0xffec;
        default: break;
    }
    // Punctuation (OEM keys): the unshifted character on the current layout.
    UINT ch = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_CHAR) & 0x7fff;
    if (ch >= 0x20 && ch < 0x7f) return (uint32_t)(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
    if (ch >= 0xa0) return (uint32_t)ch;   // Latin-1 keysyms equal their code points
    return 0;
}

// ---------------------------------------------------------------------------- window

class Win32Window final : public Window {
public:
    Win32Window(Application& app, const WindowDesc& desc);
    ~Win32Window() override { close(); }

    const char* platform_name() const override { return "win32"; }
    Size  size() const override { return {pw_ / scale_, ph_ / scale_}; }
    float scale() const override { return scale_; }
    bool  maximized() const override { return maximized_; }
    bool  active() const override { return active_; }

    // DWM draws the shadow; there is never a margin.
    void   set_shadow_insets(Insets, float) override {}
    Insets insets() const override { return {}; }
    bool   native_shadow() const override { return true; }

    void set_title(const std::string& t) override { if (hwnd_) SetWindowTextW(hwnd_, widen(t).c_str()); }
    // Deferred: these can run the system's modal loops or resize the window synchronously,
    // which must not happen inside the widget event handler that asked for them.
    void minimize() override { post_show(SW_MINIMIZE); }
    void set_maximized(bool m) override { post_show(m ? SW_MAXIMIZE : SW_RESTORE); }
    void begin_move() override { post_nc_drag(HTCAPTION); }
    void begin_resize(unsigned edges) override;
    void show_window_menu(Point p) override;
    void set_cursor(MouseCursor s) override;
    void request_redraw() override { dirty_ = true; if (hwnd_) pacer_.request(); }
    void close() override;

    NativeSurface native_surface() const override {
        NativeSurface ns{NativeSurface::Kind::Win32};
        ns.display = GetModuleHandleW(nullptr);
        ns.surface = hwnd_;
        return ns;
    }
    SoftwareBuffer acquire_software_buffer() override;
    void present_software_buffer() override;
    void will_present_gpu() override { presented_ = true; }

    static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

private:
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void emit(const Event& e) { if (on_event) on_event(e); }
    void paint_now();
    void post_show(int cmd);
    void post_nc_drag(WPARAM hit);
    void update_scale(UINT dpi);
    void pointer_button(PointerEvent::Type type, PointerButton b, LPARAM lp);
    Point logical(LPARAM lp) const { return {GET_X_LPARAM(lp) / scale_, GET_Y_LPARAM(lp) / scale_}; }
    void begin_modal_pump();
    void end_modal_pump();
    void free_dib();

    Application& app_;
    HWND  hwnd_ = nullptr;
    Size  min_size_;
    int   pw_ = 0, ph_ = 0;     // client size, physical px
    float scale_ = 1;
    bool  maximized_ = false, minimized_ = false, active_ = false;
    bool  tracking_leave_ = false;
    int   buttons_down_ = 0;
    MouseCursor cursor_ = MouseCursor::Default;
    POINT press_screen_{};      // last button press, for move/resize/menu requests
    platform::ClickTracker clicks_;
    wchar_t high_surrogate_ = 0;

    bool dirty_ = false, presented_ = false, cloaked_ = false;
    int  modal_depth_ = 0;
    platform::FramePacer pacer_{app_, [this] { paint_now(); }};

    // Software rendering: a top-down 32-bit DIB section, blitted to the window on present.
    HDC     dib_dc_ = nullptr;
    HBITMAP dib_ = nullptr, dib_old_ = nullptr;
    void*   dib_bits_ = nullptr;
    int     dib_w_ = 0, dib_h_ = 0;
};

ATOM register_class() {
    static ATOM atom = 0;
    if (atom) return atom;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.style = CS_OWNDC;   // a stable DC for GL and for presenting
    wc.lpfnWndProc = &Win32Window::wndproc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, cursor_resource(MouseCursor::Default));
    wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));   // app icon if linked in
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));   // IDI_APPLICATION
    wc.lpszClassName = kClassName;
    atom = RegisterClassExW(&wc);
    return atom;
}

Win32Window::Win32Window(Application& app, const WindowDesc& desc) : app_(app), min_size_(desc.min_size) {
    init_dpi_awareness();
    register_class();
    clicks_.interval_ms = GetDoubleClickTime();

    // Thick frame + caption: DWM shadow, snapping and animations. WM_NCCALCSIZE hides the frame.
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, kClassName, widen(desc.title).c_str(), style,
                            CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, nullptr, nullptr,
                            GetModuleHandleW(nullptr), this);
    if (!hwnd_) { fprintf(stderr, "uitk: CreateWindowEx failed (%lu)\n", GetLastError()); return; }

    update_scale(window_dpi(hwnd_));
    // Centre on the work area of the monitor the window opened on, at the requested size.
    int w = (int)std::lround(desc.size.width * scale_), h = (int)std::lround(desc.size.height * scale_);
    MONITORINFO mi{sizeof mi};
    GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT wa = mi.rcWork;
    w = std::min<int>(w, wa.right - wa.left);
    h = std::min<int>(h, wa.bottom - wa.top);
    SetWindowPos(hwnd_, nullptr, wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2,
                 w, h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // No visible border line (Windows 11), rounded corners, and the shadow (a 1 px frame margin
    // is enough for DWM to treat the window as framed).
    COLORREF none = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &none, sizeof none);
    int corners = 2;   // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);
    MARGINS margins{0, 0, 1, 0};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);

    DWM_TIMING_INFO ti{};
    ti.cbSize = sizeof ti;
    if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &ti)) && ti.rateRefresh.uiDenominator)
        pacer_.set_refresh_rate((double)ti.rateRefresh.uiNumerator / ti.rateRefresh.uiDenominator);

    // Show on the next loop iteration, once the app has attached its callbacks. Cloaked until
    // the first frame is drawn, so the window never flashes an unpainted background.
    HWND h_ = hwnd_;
    bool* cloaked = &cloaked_;
    app_.post([h_, cloaked] {
        if (!IsWindow(h_)) return;
        BOOL on = TRUE;
        if (SUCCEEDED(DwmSetWindowAttribute(h_, DWMWA_CLOAK, &on, sizeof on))) *cloaked = true;
        ShowWindow(h_, SW_SHOWNORMAL);
        UpdateWindow(h_);
    });
}

void Win32Window::close() {
    if (!hwnd_) return;
    HWND h = hwnd_;
    free_dib();
    SetWindowLongPtrW(h, GWLP_USERDATA, 0);   // no more messages reach this object
    hwnd_ = nullptr;
    DestroyWindow(h);
}

void Win32Window::update_scale(UINT dpi) {
    float s = env_scale();
    scale_ = s > 0 ? s : dpi / 96.f;
}

void Win32Window::post_show(int cmd) {
    HWND h = hwnd_;
    app_.post([h, cmd] { if (IsWindow(h)) ShowWindow(h, cmd); });
}

void Win32Window::post_nc_drag(WPARAM hit) {
    if (!hwnd_) return;
    ReleaseCapture();
    buttons_down_ = 0;
    // Posted: DefWindowProc then runs the system move/size loop (with snapping) after the
    // current event handler has returned.
    PostMessageW(hwnd_, WM_NCLBUTTONDOWN, hit, MAKELPARAM(press_screen_.x, press_screen_.y));
}

void Win32Window::begin_resize(unsigned e) {
    WPARAM hit = HTCAPTION;
    switch (e) {
        case EDGE_TOP | EDGE_LEFT:     hit = HTTOPLEFT; break;
        case EDGE_TOP | EDGE_RIGHT:    hit = HTTOPRIGHT; break;
        case EDGE_BOTTOM | EDGE_LEFT:  hit = HTBOTTOMLEFT; break;
        case EDGE_BOTTOM | EDGE_RIGHT: hit = HTBOTTOMRIGHT; break;
        case EDGE_TOP:    hit = HTTOP; break;
        case EDGE_BOTTOM: hit = HTBOTTOM; break;
        case EDGE_LEFT:   hit = HTLEFT; break;
        case EDGE_RIGHT:  hit = HTRIGHT; break;
        default: return;
    }
    post_nc_drag(hit);
}

void Win32Window::show_window_menu(Point) {
    if (!hwnd_) return;
    HWND h = hwnd_;
    POINT at = press_screen_;
    ReleaseCapture();
    buttons_down_ = 0;
    app_.post([h, at] {
        if (!IsWindow(h)) return;
        HMENU menu = GetSystemMenu(h, FALSE);
        if (!menu) return;
        bool zoomed = IsZoomed(h);
        auto enable = [&](UINT id, bool on) { EnableMenuItem(menu, id, MF_BYCOMMAND | (on ? MF_ENABLED : MF_GRAYED)); };
        enable(SC_RESTORE, zoomed);
        enable(SC_MOVE, !zoomed);
        enable(SC_SIZE, !zoomed);
        enable(SC_MINIMIZE, true);
        enable(SC_MAXIMIZE, !zoomed);
        enable(SC_CLOSE, true);
        SetMenuDefaultItem(menu, SC_CLOSE, FALSE);
        UINT cmd = (UINT)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                        at.x, at.y, 0, h, nullptr);
        if (cmd) PostMessageW(h, WM_SYSCOMMAND, cmd, 0);
    });
}

void Win32Window::set_cursor(MouseCursor s) {
    if (s == cursor_) return;
    cursor_ = s;
    if (!hwnd_) return;
    POINT p; RECT r;
    if (GetCursorPos(&p) && ScreenToClient(hwnd_, &p) && GetClientRect(hwnd_, &r) && PtInRect(&r, p))
        SetCursor(LoadCursorW(nullptr, cursor_resource(s)));
}

void Win32Window::begin_modal_pump() {
    // System modal loops (move/size, menus) don't return to our loop; tick the app from a timer.
    if (modal_depth_++ == 0 && hwnd_) SetTimer(hwnd_, kPumpTimer, USER_TIMER_MINIMUM, nullptr);
}

void Win32Window::end_modal_pump() {
    if (modal_depth_ > 0 && --modal_depth_ == 0 && hwnd_) KillTimer(hwnd_, kPumpTimer);
}

void Win32Window::paint_now() {
    if (!hwnd_ || minimized_ || !dirty_ || pw_ <= 0 || ph_ <= 0) return;
    dirty_ = false;
    pacer_.frame_drawn();
    presented_ = false;
    run_frame();   // sets the frame clock, calls on_frame, keeps animations going
    if (cloaked_) {
        BOOL off = FALSE;
        DwmSetWindowAttribute(hwnd_, DWMWA_CLOAK, &off, sizeof off);
        cloaked_ = false;
    }
    if (app_.loop_mode() == LoopMode::Continuous) request_redraw();
}

void Win32Window::pointer_button(PointerEvent::Type type, PointerButton b, LPARAM lp) {
    PointerEvent e;
    e.type = type;
    e.pos = logical(lp);
    e.button = b;
    e.mods = current_mods();
    e.time_ms = (uint32_t)GetMessageTime();
    if (type == PointerEvent::Type::Down) {
        DWORD mp = GetMessagePos();
        press_screen_ = {GET_X_LPARAM(mp), GET_Y_LPARAM(mp)};
        if (buttons_down_++ == 0) SetCapture(hwnd_);
        e.click_count = clicks_.press(b, e.pos, e.time_ms);
    } else {
        if (buttons_down_ > 0 && --buttons_down_ == 0) ReleaseCapture();
    }
    emit(e);
}

// ---------------------------------------------------------------------------- software buffer

void Win32Window::free_dib() {
    if (dib_dc_) {
        if (dib_old_) SelectObject(dib_dc_, dib_old_);
        DeleteDC(dib_dc_);
    }
    if (dib_) DeleteObject(dib_);
    dib_dc_ = nullptr; dib_ = dib_old_ = nullptr; dib_bits_ = nullptr; dib_w_ = dib_h_ = 0;
}

SoftwareBuffer Win32Window::acquire_software_buffer() {
    if (!hwnd_ || pw_ <= 0 || ph_ <= 0) return {};
    if (!dib_ || dib_w_ != pw_ || dib_h_ != ph_) {
        free_dib();
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = pw_;
        bi.bmiHeader.biHeight = -ph_;   // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        dib_ = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &dib_bits_, nullptr, 0);
        if (!dib_) { fprintf(stderr, "uitk: CreateDIBSection failed\n"); return {}; }
        dib_dc_ = CreateCompatibleDC(nullptr);
        dib_old_ = (HBITMAP)SelectObject(dib_dc_, dib_);
        dib_w_ = pw_; dib_h_ = ph_;
    }
    GdiFlush();   // GDI must be done with the bitmap before the CPU writes to it
    return {static_cast<uint32_t*>(dib_bits_), dib_w_, dib_h_, dib_w_ * 4};
}

void Win32Window::present_software_buffer() {
    if (!hwnd_ || !dib_dc_) return;
    HDC dc = GetDC(hwnd_);
    BitBlt(dc, 0, 0, dib_w_, dib_h_, dib_dc_, 0, 0, SRCCOPY);
    ReleaseDC(hwnd_, dc);
    presented_ = true;
}

// ---------------------------------------------------------------------------- messages

LRESULT CALLBACK Win32Window::wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* self = static_cast<Win32Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = h;
    }
    auto* self = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!self) return DefWindowProcW(h, msg, wp, lp);
    return self->handle(msg, wp, lp);
}

LRESULT Win32Window::handle(UINT msg, WPARAM wp, LPARAM lp) {
    HWND h = hwnd_;
    switch (msg) {
        case WM_NCCALCSIZE:
            if (wp) {
                // Client area = whole window. Maximized windows are positioned so the (invisible)
                // frame hangs off-screen; pull the client back to the monitor's work area.
                if (IsZoomed(h)) {
                    auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
                    MONITORINFO mi{sizeof mi};
                    if (GetMonitorInfoW(MonitorFromRect(&p->rgrc[0], MONITOR_DEFAULTTONEAREST), &mi))
                        p->rgrc[0] = mi.rcWork;
                }
                return 0;
            }
            break;
        case WM_NCHITTEST:
            return HTCLIENT;   // move/resize are started by the widgets (WindowShell, HeaderBar)
        case WM_NCACTIVATE:
            // Keep DWM from repainting a (non-existent) inactive frame.
            return DefWindowProcW(h, msg, wp, -1);
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(h, &ps);
            EndPaint(h, &ps);
            dirty_ = true;
            paint_now();
            return 0;
        }
        case WM_SIZE: {
            if (wp == SIZE_MINIMIZED) { minimized_ = true; return 0; }
            bool was_min = minimized_;
            minimized_ = false;
            int w = LOWORD(lp), hh = HIWORD(lp);
            bool m = wp == SIZE_MAXIMIZED;
            if (m != maximized_) { maximized_ = m; emit(StateEvent{m}); }
            if (w != pw_ || hh != ph_ || was_min) {
                pw_ = w; ph_ = hh;
                emit(ResizeEvent{size(), scale_});
                // Paint right away: during an interactive resize our loop isn't running, and the
                // system shows whatever the window holds when this message returns.
                dirty_ = true;
                if (IsWindowVisible(h)) paint_now();
            }
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
            mm->ptMinTrackSize.x = (LONG)std::lround(min_size_.width * scale_);
            mm->ptMinTrackSize.y = (LONG)std::lround(min_size_.height * scale_);
            return 0;
        }
        case WM_DPICHANGED: {
            update_scale(HIWORD(wp));
            auto* r = reinterpret_cast<RECT*>(lp);
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            emit(ResizeEvent{size(), scale_});
            request_redraw();
            return 0;
        }
        case WM_ENTERSIZEMOVE: case WM_ENTERMENULOOP:
            begin_modal_pump();
            return 0;
        case WM_EXITSIZEMOVE: case WM_EXITMENULOOP:
            end_modal_pump();
            return 0;
        case WM_TIMER:
            if (wp == kPumpTimer) { app_.process_pending(); return 0; }
            break;
        case WM_SETFOCUS:
            if (!active_) { active_ = true; emit(FocusEvent{true}); }
            return 0;
        case WM_KILLFOCUS:
            if (active_) { active_ = false; emit(FocusEvent{false}); }
            return 0;
        case WM_CLOSE:
            emit(CloseEvent{});
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursorW(nullptr, cursor_resource(cursor_))); return TRUE; }
            break;

        // ---- pointer ----
        case WM_MOUSEMOVE: {
            if (!tracking_leave_) {
                TRACKMOUSEEVENT tme{sizeof tme, TME_LEAVE, h, 0};
                TrackMouseEvent(&tme);
                tracking_leave_ = true;
                PointerEvent en;
                en.type = PointerEvent::Type::Enter;
                en.pos = logical(lp);
                en.mods = current_mods();
                emit(en);
            }
            PointerEvent e;
            e.type = PointerEvent::Type::Move;
            e.pos = logical(lp);
            e.mods = current_mods();
            e.time_ms = (uint32_t)GetMessageTime();
            emit(e);
            return 0;
        }
        case WM_MOUSELEAVE: {
            tracking_leave_ = false;
            PointerEvent e;
            e.type = PointerEvent::Type::Leave;
            e.mods = current_mods();
            emit(e);
            return 0;
        }
        case WM_LBUTTONDOWN: pointer_button(PointerEvent::Type::Down, PointerButton::Left, lp); return 0;
        case WM_LBUTTONUP:   pointer_button(PointerEvent::Type::Up, PointerButton::Left, lp); return 0;
        case WM_RBUTTONDOWN: pointer_button(PointerEvent::Type::Down, PointerButton::Right, lp); return 0;
        case WM_RBUTTONUP:   pointer_button(PointerEvent::Type::Up, PointerButton::Right, lp); return 0;
        case WM_MBUTTONDOWN: pointer_button(PointerEvent::Type::Down, PointerButton::Middle, lp); return 0;
        case WM_MBUTTONUP:   pointer_button(PointerEvent::Type::Up, PointerButton::Middle, lp); return 0;
        case WM_XBUTTONDOWN: pointer_button(PointerEvent::Type::Down, PointerButton::Other, lp); return TRUE;
        case WM_XBUTTONUP:   pointer_button(PointerEvent::Type::Up, PointerButton::Other, lp); return TRUE;
        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lp) != h) buttons_down_ = 0;
            return 0;
        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: {
            POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};   // screen coordinates
            ScreenToClient(h, &p);
            UINT lines = 3;
            SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
            if (lines == WHEEL_PAGESCROLL || lines == 0) lines = 3;
            // 15 px per notch at the default 3 lines, matching the Linux backends; precision
            // touchpads send fractions of a notch, which scroll smoothly.
            float notches = GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
            ScrollEvent se;
            se.pos = {p.x / scale_, p.y / scale_};
            se.mods = current_mods();
            if (msg == WM_MOUSEWHEEL) se.dy = -notches * 5.f * (float)lines;
            else se.dx = notches * 5.f * (float)lines;
            emit(se);
            return 0;
        }

        // ---- keyboard ----
        case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_KEYUP: case WM_SYSKEYUP: {
            bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
            KeyEvent ke;
            ke.pressed = down;
            ke.repeat = down && (lp & (1 << 30)) != 0;
            ke.keysym = keysym_for(wp, lp);
            ke.mods = current_mods();
            if (ke.keysym) emit(ke);
            // Alt+F4 closes and Alt+Space opens the window menu, as everywhere on Windows.
            // Anything else involving Alt/F10 is ours (menu bar mnemonics), so the system must
            // not enter its menu mode.
            if (msg == WM_SYSKEYDOWN && (wp == VK_F4 || wp == VK_SPACE)) return DefWindowProcW(h, msg, wp, lp);
            return 0;
        }
        case WM_SYSCOMMAND:
            // Alt or F10 alone: don't activate the (invisible) system menu bar.
            if ((wp & 0xfff0) == SC_KEYMENU && lp == 0) return 0;
            break;
        case WM_SYSCHAR:
            // Alt+Space opens the window menu (via SC_KEYMENU); other Alt+letters are ours as
            // keys (menu mnemonics), so swallow them here (no beep).
            if (wp == L' ') break;
            return 0;
        case WM_CHAR: {
            wchar_t c = (wchar_t)wp;
            if (c >= 0xd800 && c <= 0xdbff) { high_surrogate_ = c; return 0; }
            std::wstring units;
            if (c >= 0xdc00 && c <= 0xdfff) {
                if (!high_surrogate_) return 0;
                units = {high_surrogate_, c};
            } else {
                units = {c};
            }
            high_surrogate_ = 0;
            // Control characters (Backspace, Enter, Ctrl+letter, ...) come as keys instead.
            if (units.size() == 1 && (c < 0x20 || c == 0x7f)) return 0;
            emit(TextEvent{narrow(units)});
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

} // namespace

std::unique_ptr<Window> create_window(Application& app, const WindowDesc& desc) {
    auto w = std::make_unique<Win32Window>(app, desc);
    if (!w->native_surface().surface) return nullptr;
    return w;
}

} // namespace uitk::win32

namespace uitk {

std::unique_ptr<Window> Window::create(Application& app, const WindowDesc& desc) {
    return win32::create_window(app, desc);
}

} // namespace uitk
