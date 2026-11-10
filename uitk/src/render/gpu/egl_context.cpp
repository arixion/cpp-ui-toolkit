// Linux GL contexts: OpenGL ES 3 via EGL (Wayland: wl_egl_window, X11: native window,
// offscreen: Mesa's surfaceless platform).
#include "gl_context.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <wayland-egl.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"
#include "uitk/platform/window.h"

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

namespace uitk::gpu {
namespace {

const char* egl_error() {
    switch (eglGetError()) {
        case EGL_SUCCESS: return "EGL_SUCCESS";
        case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
        case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
        case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
        case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
        default: return "EGL error";
    }
}

// Display, config and context; shared by the window and offscreen contexts.
struct Egl {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig  config = nullptr;
    EGLContext context = EGL_NO_CONTEXT;

    bool init(EGLenum platform, void* native_display, EGLint surface_type, unsigned long want_visual = 0) {
        display = eglGetPlatformDisplay(platform, native_display, nullptr);
        if (display == EGL_NO_DISPLAY) { fprintf(stderr, "uitk: eglGetPlatformDisplay failed\n"); return false; }
        EGLint major, minor;
        if (!eglInitialize(display, &major, &minor)) { fprintf(stderr, "uitk: eglInitialize: %s\n", egl_error()); return false; }
        if (!eglBindAPI(EGL_OPENGL_ES_API)) return false;

        const EGLint attrs[] = {
            EGL_SURFACE_TYPE, surface_type, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_STENCIL_SIZE, 8, EGL_NONE,
        };
        EGLint n = 0;
        eglChooseConfig(display, attrs, nullptr, 0, &n);
        std::vector<EGLConfig> configs((size_t)n);
        eglChooseConfig(display, attrs, configs.data(), n, &n);
        config = nullptr;
        for (EGLConfig c : configs) {
            EGLint vid = 0, a = 0;
            eglGetConfigAttrib(display, c, EGL_NATIVE_VISUAL_ID, &vid);
            eglGetConfigAttrib(display, c, EGL_ALPHA_SIZE, &a);
            if (a != 8) continue;
            if (want_visual && (unsigned long)vid != want_visual) continue;   // X11: must match the window
            config = c;
            break;
        }
        if (!config) { fprintf(stderr, "uitk: no suitable EGL config\n"); return false; }

        const EGLint ctx_attrs[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attrs);
        if (context == EGL_NO_CONTEXT) { fprintf(stderr, "uitk: eglCreateContext: %s\n", egl_error()); return false; }
        return true;
    }

    void destroy() {
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
            // Do not eglTerminate: the display may be shared with other windows in-process.
        }
        context = EGL_NO_CONTEXT;
    }
};

class EglWindowContext final : public GlWindowContext {
public:
    explicit EglWindowContext(Window& w) : window_(w) {}
    ~EglWindowContext() override {
        if (egl_.display != EGL_NO_DISPLAY) {
            eglMakeCurrent(egl_.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (surface_ != EGL_NO_SURFACE) eglDestroySurface(egl_.display, surface_);
        }
        if (wl_window_) wl_egl_window_destroy(wl_window_);
        egl_.destroy();
    }

    bool init() {
        NativeSurface ns = window_.native_surface();
        Size px = window_.pixel_size();
        pw_ = std::max(1, (int)px.width); ph_ = std::max(1, (int)px.height);
        if (ns.kind == NativeSurface::Kind::Wayland) {
            if (!egl_.init(EGL_PLATFORM_WAYLAND_KHR, ns.display, EGL_WINDOW_BIT)) return false;
            // Defer the window surface to the first frame: Mesa allocates the first back buffer
            // at creation, and the window's final size (e.g. with its shadow margin) is only
            // known once the app has finished setting the window up.
            if (eglMakeCurrent(egl_.display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_.context)) {
                wl_surface_ = static_cast<wl_surface*>(ns.surface);
                return true;
            }
            // No surfaceless contexts: create the surface now and resize it on the first frame.
            wl_window_ = wl_egl_window_create(static_cast<wl_surface*>(ns.surface), pw_, ph_);
            surface_ = eglCreatePlatformWindowSurface(egl_.display, egl_.config, wl_window_, nullptr);
        } else if (ns.kind == NativeSurface::Kind::X11) {
            if (!egl_.init(EGL_PLATFORM_X11_KHR, ns.display, EGL_WINDOW_BIT, ns.visual_id)) return false;
            x_window_ = ns.window;
            surface_ = eglCreatePlatformWindowSurface(egl_.display, egl_.config, &x_window_, nullptr);
        } else {
            return false;
        }
        if (surface_ == EGL_NO_SURFACE) {
            fprintf(stderr, "uitk: eglCreatePlatformWindowSurface: %s\n", egl_error()); return false;
        }
        if (!eglMakeCurrent(egl_.display, surface_, surface_, egl_.context)) {
            fprintf(stderr, "uitk: eglMakeCurrent: %s\n", egl_error()); return false;
        }
        // Frame pacing is done by the window (Wayland frame callbacks / X11 timer); never block here.
        eglSwapInterval(egl_.display, 0);
        return true;
    }

    bool make_current(int w, int h) override {
        if (surface_ == EGL_NO_SURFACE && wl_surface_) {   // deferred Wayland surface
            wl_window_ = wl_egl_window_create(wl_surface_, w, h);
            surface_ = eglCreatePlatformWindowSurface(egl_.display, egl_.config, wl_window_, nullptr);
            if (surface_ == EGL_NO_SURFACE) {
                fprintf(stderr, "uitk: eglCreatePlatformWindowSurface: %s\n", egl_error()); return false;
            }
            eglMakeCurrent(egl_.display, surface_, surface_, egl_.context);
            eglSwapInterval(egl_.display, 0);
            pw_ = w; ph_ = h;
        } else if (wl_window_ && (w != pw_ || h != ph_)) {
            wl_egl_window_resize(wl_window_, w, h, 0, 0);   // before the next back buffer is fetched
        }
        pw_ = w; ph_ = h;
        eglMakeCurrent(egl_.display, surface_, surface_, egl_.context);
        if (x_window_) {
            // X11 EGL surfaces track the server-side window size immediately; if the WM has
            // resized us but the ConfigureNotify is still queued, skip this frame rather than
            // draw it at the stale size (the resize event triggers a redraw right after).
            EGLint ew = 0, eh = 0;
            eglQuerySurface(egl_.display, surface_, EGL_WIDTH, &ew);
            eglQuerySurface(egl_.display, surface_, EGL_HEIGHT, &eh);
            if (ew != w || eh != h) return false;
        }
        return true;
    }

    void swap() override {
        if (!eglSwapBuffers(egl_.display, surface_)) fprintf(stderr, "uitk: eglSwapBuffers: %s\n", egl_error());
    }

    sk_sp<const GrGLInterface> interface() override { return GrGLInterfaces::MakeEGL(); }
    const char* api() const override { return "gles"; }
    std::string renderer() const override;

private:
    Window& window_;
    Egl egl_;
    EGLSurface surface_ = EGL_NO_SURFACE;
    wl_egl_window* wl_window_ = nullptr;
    wl_surface*    wl_surface_ = nullptr;   // set when the EGL surface is created lazily
    unsigned long x_window_ = 0;
    int pw_ = 0, ph_ = 0;
};

class EglOffscreenContext final : public GlOffscreenContext {
public:
    ~EglOffscreenContext() override { egl_.destroy(); }
    bool init() {
        // Headless: Mesa's surfaceless platform needs no window system at all.
        if (!egl_.init(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, EGL_PBUFFER_BIT)) return false;
        return make_current();
    }
    bool make_current() override {
        if (eglMakeCurrent(egl_.display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_.context)) return true;
        fprintf(stderr, "uitk: surfaceless eglMakeCurrent: %s\n", egl_error());
        return false;
    }
    sk_sp<const GrGLInterface> interface() override { return GrGLInterfaces::MakeEGL(); }
    const char* api() const override { return "gles"; }
    std::string renderer() const override;

private:
    Egl egl_;
};

} // namespace

std::string gl_renderer() {
    const char* r = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return r ? r : "unknown";
}
std::string EglWindowContext::renderer() const { return gl_renderer(); }
std::string EglOffscreenContext::renderer() const { return gl_renderer(); }

std::unique_ptr<GlWindowContext> make_gl_window_context(Window& window) {
    auto c = std::make_unique<EglWindowContext>(window);
    return c->init() ? std::move(c) : nullptr;
}

std::unique_ptr<GlOffscreenContext> make_gl_offscreen_context() {
    auto c = std::make_unique<EglOffscreenContext>();
    return c->init() ? std::move(c) : nullptr;
}

} // namespace uitk::gpu
