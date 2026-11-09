// uitk_render_gpu: Skia Ganesh on OpenGL. The GL context comes from the platform (gl_context.h:
// EGL/GLES on Linux, WGL/desktop GL on Windows). Falls back to the CPU raster renderer when GL
// is unavailable, or when UITK_RENDERER=cpu.
#include "../skia/skia_internal.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "gl_context.h"
#include "include/core/SkColorSpace.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"

namespace uitk {
namespace {

constexpr unsigned kGL_RGBA8 = 0x8058;

// Skia context over the current GL context.
sk_sp<GrDirectContext> make_skia_context(sk_sp<const GrGLInterface> iface) {
    if (!iface) { fprintf(stderr, "uitk: no GL interface\n"); return nullptr; }
    auto gr = GrDirectContexts::MakeGL(iface);
    if (!gr) fprintf(stderr, "uitk: GrDirectContexts::MakeGL failed\n");
    return gr;
}

// Begins a frame on `surface`: cleared, scaled to logical px, origin at `origin`.
Canvas* begin_canvas(SkSurface* surface, std::optional<Canvas>& canvas, float scale, Point origin) {
    SkCanvas* c = surface->getCanvas();
    c->restoreToCount(1);
    c->resetMatrix();
    c->clear(SK_ColorTRANSPARENT);
    c->scale(scale, scale);
    c->translate(origin.x, origin.y);
    canvas.emplace(c);
    return &*canvas;
}

// ---------------------------------------------------------------------------- window renderer

class GlWindowRenderer final : public WindowRenderer {
public:
    GlWindowRenderer(Window& w, std::unique_ptr<gpu::GlWindowContext> gl) : window_(w), gl_(std::move(gl)) {}
    ~GlWindowRenderer() override {
        surface_.reset();
        if (gr_) { gr_->abandonContext(); gr_.reset(); }
        gl_.reset();
    }

    bool init() {
        gr_ = make_skia_context(gl_->interface());
        renderer_ = gl_->renderer();
        return gr_ != nullptr;
    }

    Canvas* begin_frame() override {
        Size px = window_.pixel_size();
        int w = std::max(1, (int)px.width), h = std::max(1, (int)px.height);
        if (!gl_->make_current(w, h)) return nullptr;
        if (w != pw_ || h != ph_ || !surface_) {
            pw_ = w; ph_ = h;
            GrGLFramebufferInfo fbi{0, kGL_RGBA8};
            auto rt = GrBackendRenderTargets::MakeGL(w, h, 0, 8, fbi);
            surface_ = SkSurfaces::WrapBackendRenderTarget(gr_.get(), rt, kBottomLeft_GrSurfaceOrigin,
                                                           kRGBA_8888_SkColorType, nullptr, nullptr);
            if (!surface_) { fprintf(stderr, "uitk: cannot wrap GL framebuffer\n"); return nullptr; }
        }
        // Origin = the window's top-left; the shadow margin lies at negative coordinates.
        Insets in = window_.insets();
        return begin_canvas(surface_.get(), canvas_, window_.scale(), {in.left, in.top});
    }

    void end_frame() override {
        canvas_.reset();
        gr_->flushAndSubmit(surface_.get());
        window_.will_present_gpu();
        gl_->swap();
    }

    std::string description() const override {
        return std::string("skia-ganesh-") + gl_->api() + " (" + renderer_ + ")";
    }

private:
    Window& window_;
    std::unique_ptr<gpu::GlWindowContext> gl_;
    sk_sp<GrDirectContext> gr_;
    std::string renderer_;
    int pw_ = 0, ph_ = 0;
    sk_sp<SkSurface> surface_;
    std::optional<Canvas> canvas_;
};

// ---------------------------------------------------------------------------- offscreen renderer

class GlOffscreenRenderer final : public OffscreenRenderer {
public:
    GlOffscreenRenderer(Size logical, float scale, std::unique_ptr<gpu::GlOffscreenContext> gl)
        : scale_(scale), gl_(std::move(gl)) {
        w_ = std::max(1, (int)(logical.width * scale + 0.5f));
        h_ = std::max(1, (int)(logical.height * scale + 0.5f));
    }
    ~GlOffscreenRenderer() override {
        surface_.reset();
        if (gr_) { gr_->abandonContext(); gr_.reset(); }
        gl_.reset();
    }

    bool init() {
        gr_ = make_skia_context(gl_->interface());
        renderer_ = gl_->renderer();
        if (!gr_) return false;
        surface_ = SkSurfaces::RenderTarget(gr_.get(), skgpu::Budgeted::kNo, SkImageInfo::MakeN32Premul(w_, h_));
        return surface_ != nullptr;
    }

    Canvas* begin_frame() override {
        gl_->make_current();
        return begin_canvas(surface_.get(), canvas_, scale_, {});
    }
    void end_frame() override { canvas_.reset(); gr_->flushAndSubmit(surface_.get(), GrSyncCpu::kYes); }
    bool write_png(const std::string& path) override { return skglue::write_png(surface_.get(), path); }
    std::string description() const override {
        return std::string("skia-ganesh-") + gl_->api() + " offscreen (" + renderer_ + ")";
    }

private:
    float scale_;
    int w_, h_;
    std::unique_ptr<gpu::GlOffscreenContext> gl_;
    sk_sp<GrDirectContext> gr_;
    std::string renderer_;
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
        if (auto gl = gpu::make_gl_window_context(w)) {
            auto r = std::make_unique<GlWindowRenderer>(w, std::move(gl));
            if (r->init()) return r;
        }
        fprintf(stderr, "uitk: GPU renderer unavailable, falling back to CPU raster\n");
    }
    return skglue::make_raster_window_renderer(w);
}

std::unique_ptr<OffscreenRenderer> OffscreenRenderer::create(Size logical, float scale) {
    if (!gpu_disabled()) {
        if (auto gl = gpu::make_gl_offscreen_context()) {
            auto r = std::make_unique<GlOffscreenRenderer>(logical, scale, std::move(gl));
            if (r->init()) return r;
        }
        fprintf(stderr, "uitk: GPU offscreen renderer unavailable, falling back to CPU raster\n");
    }
    return skglue::make_raster_offscreen_renderer(logical, scale);
}

} // namespace uitk
