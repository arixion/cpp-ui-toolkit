// CPU raster rendering: Skia draws straight into the window's software buffer (zero copy).
#include "skia_internal.h"

#include <cstdio>

#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/encode/SkPngEncoder.h"

namespace uitk::skglue {

namespace {

class RasterWindowRenderer final : public WindowRenderer {
public:
    explicit RasterWindowRenderer(Window& w) : window_(w) {}

    Canvas* begin_frame() override {
        SoftwareBuffer b = window_.acquire_software_buffer();
        if (!b.pixels) return nullptr;
        // Wayland ARGB8888 / X11 32-bit ZPixmap are BGRA in memory on little-endian: kBGRA_8888.
        if (!surface_ || surface_pixels_ != b.pixels || surface_->width() != b.width || surface_->height() != b.height) {
            SkImageInfo info = SkImageInfo::Make(b.width, b.height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
            surface_ = SkSurfaces::WrapPixels(info, b.pixels, (size_t)b.stride);
            surface_pixels_ = b.pixels;
        }
        if (!surface_) return nullptr;
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
        window_.present_software_buffer();
    }

    std::string description() const override { return "skia-raster"; }

private:
    Window& window_;
    sk_sp<SkSurface> surface_;
    void* surface_pixels_ = nullptr;
    std::optional<Canvas> canvas_;
};

class RasterOffscreenRenderer final : public OffscreenRenderer {
public:
    RasterOffscreenRenderer(Size logical, float scale) : scale_(scale) {
        int w = (int)(logical.width * scale + 0.5f), h = (int)(logical.height * scale + 0.5f);
        surface_ = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
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
    void end_frame() override { canvas_.reset(); }
    bool write_png(const std::string& path) override { return skglue::write_png(surface_.get(), path); }
    std::string description() const override { return "skia-raster (offscreen)"; }

private:
    float scale_;
    sk_sp<SkSurface> surface_;
    std::optional<Canvas> canvas_;
};

} // namespace

std::unique_ptr<WindowRenderer> make_raster_window_renderer(Window& w) {
    return std::make_unique<RasterWindowRenderer>(w);
}

std::unique_ptr<OffscreenRenderer> make_raster_offscreen_renderer(Size logical, float scale) {
    return std::make_unique<RasterOffscreenRenderer>(logical, scale);
}

bool write_png(SkSurface* surface, const std::string& path) {
    // readPixels works for raster and GPU surfaces alike.
    SkImageInfo info = SkImageInfo::Make(surface->width(), surface->height(),
                                         kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
    std::vector<uint8_t> px(info.computeMinByteSize());
    if (!surface->readPixels(info, px.data(), info.minRowBytes(), 0, 0)) return false;
    SkPixmap pm(info, px.data(), info.minRowBytes());
    SkFILEWStream out(path.c_str());
    if (!out.isValid()) { fprintf(stderr, "uitk: cannot write %s\n", path.c_str()); return false; }
    return SkPngEncoder::Encode(&out, pm, {});
}

} // namespace uitk::skglue
