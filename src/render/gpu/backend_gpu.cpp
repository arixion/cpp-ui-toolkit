// uitk_render_gpu: Skia Ganesh on OpenGL ES 3 via EGL (Wayland: wl_egl_window, X11: native window).
// Falls back to the CPU raster renderer when EGL/GL is unavailable.
#include "../skia/skia_internal.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <wayland-egl.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

namespace uitk {
namespace {

constexpr unsigned kGL_RGBA8 = 0x8058;

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

// Shared EGL + Skia context setup.
struct GlContext {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig  config = nullptr;
    EGLContext context = EGL_NO_CONTEXT;
    sk_sp<GrDirectContext> gr;
    std::string renderer;

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

    // Requires the context to be current.
    bool make_skia_context() {
        auto iface = GrGLInterfaces::MakeEGL();
        if (!iface) { fprintf(stderr, "uitk: GrGLInterfaces::MakeEGL failed\n"); return false; }
        gr = GrDirectContexts::MakeGL(iface);
        if (!gr) { fprintf(stderr, "uitk: GrDirectContexts::MakeGL failed\n"); return false; }
        const char* r = (const char*)glGetString(GL_RENDERER);
        renderer = r ? r : "unknown";
        return true;
    }

    void destroy() {
        if (gr) { gr->abandonContext(); gr.reset(); }
        if (display != EGL_NO_DISPLAY) {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
            // Do not eglTerminate: the display may be shared with other windows in-process.
        }
        context = EGL_NO_CONTEXT;
    }
};

// ---------------------------------------------------------------------------- window renderer

class GlWindowRenderer final : public WindowRenderer {
public:
    explicit GlWindowRenderer(Window& w) : window_(w) {}
    ~GlWindowRenderer() override {
        surface_.reset();
        if (gl_.display != EGL_NO_DISPLAY) {
            eglMakeCurrent(gl_.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (egl_surface_ != EGL_NO_SURFACE) eglDestroySurface(gl_.display, egl_surface_);
        }
        if (wl_window_) wl_egl_window_destroy(wl_window_);
        gl_.destroy();
    }

    bool init() {
        NativeSurface ns = window_.native_surface();
        Size px = window_.pixel_size();
        pw_ = std::max(1, (int)px.width); ph_ = std::max(1, (int)px.height);
        if (ns.kind == NativeSurface::Kind::Wayland) {
            if (!gl_.init(EGL_PLATFORM_WAYLAND_KHR, ns.display, EGL_WINDOW_BIT)) return false;
            // Defer the window surface to the first frame: Mesa allocates the first back buffer
            // at creation, and the window's final size (e.g. with its shadow margin) is only
            // known once the app has finished setting the window up.
            if (eglMakeCurrent(gl_.display, EGL_NO_SURFACE, EGL_NO_SURFACE, gl_.context)) {
                wl_surface_ = static_cast<wl_surface*>(ns.surface);
                return gl_.make_skia_context();
            }
            // No surfaceless contexts: create the surface now and resize it on the first frame.
            wl_window_ = wl_egl_window_create(static_cast<wl_surface*>(ns.surface), pw_, ph_);
            egl_surface_ = eglCreatePlatformWindowSurface(gl_.display, gl_.config, wl_window_, nullptr);
        } else {
            if (!gl_.init(EGL_PLATFORM_X11_KHR, ns.display, EGL_WINDOW_BIT, ns.visual_id)) return false;
            x_window_ = ns.window;
            egl_surface_ = eglCreatePlatformWindowSurface(gl_.display, gl_.config, &x_window_, nullptr);
        }
        if (egl_surface_ == EGL_NO_SURFACE) {
            fprintf(stderr, "uitk: eglCreatePlatformWindowSurface: %s\n", egl_error()); return false;
        }
        if (!eglMakeCurrent(gl_.display, egl_surface_, egl_surface_, gl_.context)) {
            fprintf(stderr, "uitk: eglMakeCurrent: %s\n", egl_error()); return false;
        }
        // Frame pacing is done by the window (Wayland frame callbacks / X11 timer); never block here.
        eglSwapInterval(gl_.display, 0);
        return gl_.make_skia_context();
    }

    Canvas* begin_frame() override {
        Size px = window_.pixel_size();
        int w = std::max(1, (int)px.width), h = std::max(1, (int)px.height);
        if (egl_surface_ == EGL_NO_SURFACE && wl_surface_) {   // deferred Wayland surface
            wl_window_ = wl_egl_window_create(wl_surface_, w, h);
            egl_surface_ = eglCreatePlatformWindowSurface(gl_.display, gl_.config, wl_window_, nullptr);
            if (egl_surface_ == EGL_NO_SURFACE) {
                fprintf(stderr, "uitk: eglCreatePlatformWindowSurface: %s\n", egl_error()); return nullptr;
            }
            eglMakeCurrent(gl_.display, egl_surface_, egl_surface_, gl_.context);
            eglSwapInterval(gl_.display, 0);
            pw_ = w; ph_ = h;
            surface_.reset();
        } else if (wl_window_ && (w != pw_ || h != ph_)) {
            wl_egl_window_resize(wl_window_, w, h, 0, 0);   // before the next back buffer is fetched
        }
        eglMakeCurrent(gl_.display, egl_surface_, egl_surface_, gl_.context);
        if (x_window_) {
            // X11 EGL surfaces track the server-side window size immediately; if the WM has
            // resized us but the ConfigureNotify is still queued, skip this frame rather than
            // draw it at the stale size (the resize event triggers a redraw right after).
            EGLint ew = 0, eh = 0;
            eglQuerySurface(gl_.display, egl_surface_, EGL_WIDTH, &ew);
            eglQuerySurface(gl_.display, egl_surface_, EGL_HEIGHT, &eh);
            if (ew != w || eh != h) return nullptr;
        }
        if (w != pw_ || h != ph_ || !surface_) {
            pw_ = w; ph_ = h;
            GrGLFramebufferInfo fbi{0, kGL_RGBA8};
            auto rt = GrBackendRenderTargets::MakeGL(w, h, 0, 8, fbi);
            surface_ = SkSurfaces::WrapBackendRenderTarget(gl_.gr.get(), rt, kBottomLeft_GrSurfaceOrigin,
                                                           kRGBA_8888_SkColorType, nullptr, nullptr);
            if (!surface_) { fprintf(stderr, "uitk: cannot wrap GL framebuffer\n"); return nullptr; }
        }
        SkCanvas* c = surface_->getCanvas();
        c->restoreToCount(1);
        c->resetMatrix();
        c->clear(SK_ColorTRANSPARENT);
        float s = window_.scale();
        c->scale(s, s);
        // Origin = the window's top-left; the shadow margin lies at negative coordinates.
        Insets in = window_.insets();
        c->translate(in.left, in.top);
        canvas_.emplace(c);
        return &*canvas_;
    }

    void end_frame() override {
        canvas_.reset();
        gl_.gr->flushAndSubmit(surface_.get());
        window_.will_present_gpu();
        if (!eglSwapBuffers(gl_.display, egl_surface_))
            fprintf(stderr, "uitk: eglSwapBuffers: %s\n", egl_error());
    }

    std::string description() const override { return "skia-ganesh-gles (" + gl_.renderer + ")"; }

private:
    Window& window_;
    GlContext gl_;
    EGLSurface egl_surface_ = EGL_NO_SURFACE;
    wl_egl_window* wl_window_ = nullptr;
    wl_surface*    wl_surface_ = nullptr;   // set when the EGL surface is created lazily
    unsigned long x_window_ = 0;
    int pw_ = 0, ph_ = 0;
    sk_sp<SkSurface> surface_;
    std::optional<Canvas> canvas_;
};

// ---------------------------------------------------------------------------- offscreen renderer

class GlOffscreenRenderer final : public OffscreenRenderer {
public:
    GlOffscreenRenderer(Size logical, float scale) : scale_(scale) {
        w_ = std::max(1, (int)(logical.width * scale + 0.5f));
        h_ = std::max(1, (int)(logical.height * scale + 0.5f));
    }
    ~GlOffscreenRenderer() override { surface_.reset(); gl_.destroy(); }

    bool init() {
        // Headless: Mesa's surfaceless platform needs no window system at all.
        if (!gl_.init(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, EGL_PBUFFER_BIT)) return false;
        if (!eglMakeCurrent(gl_.display, EGL_NO_SURFACE, EGL_NO_SURFACE, gl_.context)) {
            fprintf(stderr, "uitk: surfaceless eglMakeCurrent: %s\n", egl_error()); return false;
        }
        if (!gl_.make_skia_context()) return false;
        surface_ = SkSurfaces::RenderTarget(gl_.gr.get(), skgpu::Budgeted::kNo,
                                            SkImageInfo::MakeN32Premul(w_, h_));
        return surface_ != nullptr;
    }

    Canvas* begin_frame() override {
        SkCanvas* c = surface_->getCanvas();
        c->restoreToCount(1);
        c->resetMatrix();
        c->clear(SK_ColorTRANSPARENT);
        c->scale(scale_, scale_);
        canvas_.emplace(c);
        return &*canvas_;
    }
    void end_frame() override { canvas_.reset(); gl_.gr->flushAndSubmit(surface_.get(), GrSyncCpu::kYes); }
    bool write_png(const std::string& path) override { return skglue::write_png(surface_.get(), path); }
    std::string description() const override { return "skia-ganesh-gles offscreen (" + gl_.renderer + ")"; }

private:
    float scale_;
    int w_, h_;
    GlContext gl_;
    sk_sp<SkSurface> surface_;
    std::optional<Canvas> canvas_;
};

bool gpu_disabled() {
    const char* v = getenv("UITK_RENDERER");
    return v && std::string(v) == "cpu";
}

} // namespace

std::unique_ptr<WindowRenderer> WindowRenderer::create(Window& w) {
    if (!gpu_disabled()) {
        auto r = std::make_unique<GlWindowRenderer>(w);
        if (r->init()) return r;
        fprintf(stderr, "uitk: GPU renderer unavailable, falling back to CPU raster\n");
    }
    return skglue::make_raster_window_renderer(w);
}

std::unique_ptr<OffscreenRenderer> OffscreenRenderer::create(Size logical, float scale) {
    if (!gpu_disabled()) {
        auto r = std::make_unique<GlOffscreenRenderer>(logical, scale);
        if (r->init()) return r;
        fprintf(stderr, "uitk: GPU offscreen renderer unavailable, falling back to CPU raster\n");
    }
    return skglue::make_raster_offscreen_renderer(logical, scale);
}

} // namespace uitk
