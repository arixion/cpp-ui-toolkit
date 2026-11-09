// Images: raster via Skia codecs (PNG/JPEG/WebP), vector via the Skia SVG module.
#include "skia_internal.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>

#include "include/codec/SkCodec.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkData.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkStream.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/include/SkSVGSVG.h"

namespace uitk {

struct Image::Impl {
    sk_sp<SkImage>  raster;
    sk_sp<SkSVGDOM> svg;
    Size            size;
    // Rendered copy at the last device-pixel size it was drawn at (one entry: images are
    // nearly always drawn at a single size). Vector images are cached untinted; the tint is
    // applied while drawing, so theme changes don't invalidate it.
    mutable sk_sp<SkImage> device_copy;
    mutable int device_w = 0, device_h = 0;
};

namespace {

void register_codecs() {
    static std::once_flag once;
    std::call_once(once, [] {
        SkCodecs::Register(SkPngDecoder::Decoder());
        SkCodecs::Register(SkJpegDecoder::Decoder());
        SkCodecs::Register(SkWebpDecoder::Decoder());
    });
}

bool ends_with(const std::string& s, const char* suffix) {
    size_t n = strlen(suffix);
    return s.size() >= n && std::equal(s.end() - (long)n, s.end(), suffix,
        [](char a, char b) { return std::tolower((unsigned char)a) == b; });
}

} // namespace

Image Image::decode(const void* data, size_t len) {
    register_codecs();
    auto img = SkImages::DeferredFromEncodedData(SkData::MakeWithCopy(data, len));
    if (!img) return {};
    img = img->makeRasterImage(nullptr);   // decode now, not on first draw
    if (!img) return {};
    Image out;
    out.impl_ = std::make_shared<Impl>();
    out.impl_->raster = img;
    out.impl_->size = {(float)img->width(), (float)img->height()};
    return out;
}

Image Image::from_svg(std::string_view src) {
    SkMemoryStream stream(src.data(), src.size(), /*copyData=*/false);
    auto dom = SkSVGDOM::Builder().setFontManager(skglue::font_manager()).make(stream);
    if (!dom) return {};
    SkSize sz = dom->getRoot()->intrinsicSize(SkSVGLengthContext(SkSize::Make(16, 16)));
    if (sz.isEmpty()) sz = SkSize::Make(16, 16);
    dom->setContainerSize(sz);
    Image out;
    out.impl_ = std::make_shared<Impl>();
    out.impl_->svg = dom;
    out.impl_->size = {sz.width(), sz.height()};
    return out;
}

Image Image::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { fprintf(stderr, "uitk: cannot open image %s\n", path.c_str()); return {}; }
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Image img = ends_with(path, ".svg") ? from_svg(bytes) : decode(bytes.data(), bytes.size());
    if (!img.valid()) fprintf(stderr, "uitk: cannot decode image %s\n", path.c_str());
    return img;
}

bool Image::is_vector() const { return impl_ && impl_->svg; }
Size Image::size() const { return impl_ ? impl_->size : Size{}; }

namespace {

// Renders `im` at exactly w x h device pixels (high-quality resample / vector render).
sk_sp<SkImage> render_device_copy(const Image::Impl& im, int w, int h) {
    if (im.raster) {
        if (im.raster->width() == w && im.raster->height() == h) return im.raster;
        SkBitmap bm;
        if (!bm.tryAllocPixels(SkImageInfo::MakeN32Premul(w, h))) return nullptr;
        if (!im.raster->scalePixels(bm.pixmap(), SkSamplingOptions(SkCubicResampler::Mitchell()))) return nullptr;
        bm.setImmutable();
        return SkImages::RasterFromBitmap(bm);
    }
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
    if (!surface) return nullptr;
    SkCanvas* sc = surface->getCanvas();
    sc->clear(SK_ColorTRANSPARENT);
    sc->scale(w / im.size.width, h / im.size.height);
    im.svg->render(sc);
    return surface->makeImageSnapshot();
}

} // namespace

void Canvas::draw_image(const Image& img, const Rect& dst, std::optional<Color> tint) {
    if (!img.impl_ || dst.empty()) return;
    SkCanvas* c = skglue::sk(c_);
    const Image::Impl& im = *img.impl_;

    // Fast path (axis-aligned): draw a cached copy rendered at the exact device size,
    // pixel-snapped, 1:1. No per-frame resampling, SVG traversal or tint layer.
    const SkMatrix m = c->getTotalMatrix();
    if (m.isScaleTranslate() && m.getScaleX() > 0 && m.getScaleY() > 0) {
        SkIRect dev = m.mapRect(skglue::to_sk(dst)).round();
        int w = dev.width(), h = dev.height();
        if (w > 0 && h > 0 && w <= 4096 && h <= 4096) {
            if (!im.device_copy || im.device_w != w || im.device_h != h) {
                im.device_copy = render_device_copy(im, w, h);
                im.device_w = w; im.device_h = h;
            }
            if (im.device_copy) {
                SkPaint p;
                if (tint) p.setColorFilter(SkColorFilters::Blend(skglue::to_sk(*tint), SkBlendMode::kSrcIn));
                c->save();
                c->resetMatrix();
                c->drawImage(im.device_copy, (float)dev.x(), (float)dev.y(), SkSamplingOptions(), &p);
                c->restore();
                return;
            }
        }
    }

    int saves = c->getSaveCount();
    if (tint) {
        // Draw into a layer, then recolour every pixel with the tint, keeping its alpha.
        SkPaint layer;
        layer.setColorFilter(SkColorFilters::Blend(skglue::to_sk(*tint), SkBlendMode::kSrcIn));
        SkRect bounds = skglue::to_sk(dst);
        c->saveLayer(&bounds, &layer);
    }
    if (im.raster) {
        c->drawImageRect(im.raster, skglue::to_sk(dst), SkSamplingOptions(SkCubicResampler::Mitchell()));
    } else if (im.svg) {
        c->save();
        c->translate(dst.x, dst.y);
        c->scale(dst.width / im.size.width, dst.height / im.size.height);
        im.svg->render(c);
        c->restore();
    }
    c->restoreToCount(saves);
}

} // namespace uitk
