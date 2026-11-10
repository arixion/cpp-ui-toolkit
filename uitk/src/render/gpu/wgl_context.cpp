// Windows GL contexts: desktop OpenGL via WGL on the window's own DC (CS_OWNDC). Offscreen
// rendering uses a hidden 1x1 window for the context and draws into a Skia render target.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>

#include "gl_context.h"

#include <cstdio>

#include "include/gpu/ganesh/gl/win/GrGLMakeWinInterface.h"
#include "uitk/platform/window.h"

namespace uitk::gpu {
namespace {

using SwapIntervalFn = BOOL(WINAPI*)(int);

std::string gl_renderer() {
    const char* r = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return r ? r : "unknown";
}

// Pixel format + context on `dc`. The window is opaque (DWM draws the shadow), so no alpha
// channel is needed in the framebuffer; Skia still renders RGBA8.
HGLRC create_context(HDC dc) {
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int format = ChoosePixelFormat(dc, &pfd);
    if (!format || !SetPixelFormat(dc, format, &pfd)) {
        fprintf(stderr, "uitk: no usable WGL pixel format (%lu)\n", GetLastError());
        return nullptr;
    }
    HGLRC rc = wglCreateContext(dc);
    if (!rc) { fprintf(stderr, "uitk: wglCreateContext failed (%lu)\n", GetLastError()); return nullptr; }
    if (!wglMakeCurrent(dc, rc)) {
        fprintf(stderr, "uitk: wglMakeCurrent failed (%lu)\n", GetLastError());
        wglDeleteContext(rc);
        return nullptr;
    }
    return rc;
}

class WglWindowContext final : public GlWindowContext {
public:
    ~WglWindowContext() override {
        if (rc_) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(rc_); }
        if (dc_ && hwnd_) ReleaseDC(hwnd_, dc_);
    }

    bool init(HWND hwnd) {
        hwnd_ = hwnd;
        dc_ = GetDC(hwnd);
        if (!dc_) return false;
        rc_ = create_context(dc_);
        if (!rc_) return false;
        // Vsync: SwapBuffers waits for the next vblank, which paces continuous redraws; the
        // window's frame pacer already keeps on-demand frames to the refresh rate.
        auto swap_interval = reinterpret_cast<SwapIntervalFn>(
            reinterpret_cast<void*>(wglGetProcAddress("wglSwapIntervalEXT")));
        if (swap_interval) swap_interval(1);
        return true;
    }

    bool make_current(int, int) override {
        // The default framebuffer follows the window's client size automatically.
        return wglGetCurrentContext() == rc_ || wglMakeCurrent(dc_, rc_);
    }
    void swap() override { SwapBuffers(dc_); }
    sk_sp<const GrGLInterface> interface() override { return GrGLInterfaces::MakeWin(); }
    const char* api() const override { return "gl"; }
    std::string renderer() const override { return gl_renderer(); }

private:
    HWND  hwnd_ = nullptr;
    HDC   dc_ = nullptr;
    HGLRC rc_ = nullptr;
};

class WglOffscreenContext final : public GlOffscreenContext {
public:
    ~WglOffscreenContext() override {
        if (rc_) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(rc_); }
        if (dc_) ReleaseDC(hwnd_, dc_);
        if (hwnd_) DestroyWindow(hwnd_);
    }

    bool init() {
        static ATOM cls = [] {
            WNDCLASSW wc{};
            wc.style = CS_OWNDC;
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = L"uitk.offscreen-gl";
            return RegisterClassW(&wc);
        }();
        if (!cls) return false;
        hwnd_ = CreateWindowExW(0, L"uitk.offscreen-gl", L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
        if (!hwnd_) return false;
        dc_ = GetDC(hwnd_);
        rc_ = dc_ ? create_context(dc_) : nullptr;
        return rc_ != nullptr;
    }

    bool make_current() override { return wglGetCurrentContext() == rc_ || wglMakeCurrent(dc_, rc_); }
    sk_sp<const GrGLInterface> interface() override { return GrGLInterfaces::MakeWin(); }
    const char* api() const override { return "gl"; }
    std::string renderer() const override { return gl_renderer(); }

private:
    HWND  hwnd_ = nullptr;
    HDC   dc_ = nullptr;
    HGLRC rc_ = nullptr;
};

} // namespace

std::unique_ptr<GlWindowContext> make_gl_window_context(Window& window) {
    NativeSurface ns = window.native_surface();
    if (ns.kind != NativeSurface::Kind::Win32 || !ns.surface) return nullptr;
    auto c = std::make_unique<WglWindowContext>();
    return c->init(static_cast<HWND>(ns.surface)) ? std::move(c) : nullptr;
}

std::unique_ptr<GlOffscreenContext> make_gl_offscreen_context() {
    auto c = std::make_unique<WglOffscreenContext>();
    return c->init() ? std::move(c) : nullptr;
}

} // namespace uitk::gpu
