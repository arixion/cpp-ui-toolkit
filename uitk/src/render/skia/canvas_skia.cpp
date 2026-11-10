#include "skia_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "include/core/SkBlurTypes.h"
#include "include/core/SkImage.h"
#include "include/core/SkSurface.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathEffect.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/core/SkRRect.h"

namespace uitk {

using skglue::sk;
using skglue::to_sk;

namespace {
SkPaint to_sk_paint(const Paint& p) {
    SkPaint sp;
    sp.setColor(to_sk(p.color));
    sp.setAntiAlias(p.antialias);
    if (p.style == Paint::Style::Stroke) {
        sp.setStyle(SkPaint::kStroke_Style);
        sp.setStrokeWidth(p.stroke_width);
        if (p.dash_on > 0) {
            SkScalar intervals[2] = {p.dash_on, p.dash_off > 0 ? p.dash_off : p.dash_on};
            sp.setPathEffect(SkDashPathEffect::Make(intervals, 0));
        }
    }
    return sp;
}
} // namespace

// ---- Path ---------------------------------------------------------------------

Path::Path() : impl_(std::make_unique<Impl>()) {}
Path::~Path() = default;
Path::Path(const Path& o) : impl_(std::make_unique<Impl>(*o.impl_)) {}
Path& Path::operator=(const Path& o) { *impl_ = *o.impl_; return *this; }

Path& Path::move_to(Point p) { impl_->builder.moveTo(p.x, p.y); return *this; }
Path& Path::line_to(Point p) { impl_->builder.lineTo(p.x, p.y); return *this; }
Path& Path::quad_to(Point c, Point p) { impl_->builder.quadTo(c.x, c.y, p.x, p.y); return *this; }
Path& Path::cubic_to(Point c1, Point c2, Point p) {
    impl_->builder.cubicTo(c1.x, c1.y, c2.x, c2.y, p.x, p.y); return *this;
}
Path& Path::add_bezier(const CubicBezier& b) {
    impl_->builder.moveTo(b.p0.x, b.p0.y);
    impl_->builder.cubicTo(b.p1.x, b.p1.y, b.p2.x, b.p2.y, b.p3.x, b.p3.y);
    return *this;
}
Path& Path::add_arc(const Rect& oval, float start, float sweep) {
    impl_->builder.addArc(to_sk(oval), start, sweep);
    return *this;
}
Path& Path::close() { impl_->builder.close(); return *this; }
Path& Path::add_rect(const Rect& r) { impl_->builder.addRect(to_sk(r)); return *this; }
Path& Path::add_rrect(const Rect& r, float radius) {
    impl_->builder.addRRect(SkRRect::MakeRectXY(to_sk(r), radius, radius)); return *this;
}
Path& Path::add_circle(Point c, float radius) { impl_->builder.addCircle(c.x, c.y, radius); return *this; }

// ---- Canvas -------------------------------------------------------------------

void Canvas::save() { sk(c_)->save(); }
void Canvas::save_layer(float opacity) { sk(c_)->saveLayerAlphaf(nullptr, opacity); }
void Canvas::restore() { sk(c_)->restore(); }
void Canvas::translate(float dx, float dy) { sk(c_)->translate(dx, dy); }
void Canvas::scale(float sx, float sy) { sk(c_)->scale(sx, sy); }
void Canvas::rotate(float deg, Point c) { sk(c_)->rotate(deg, c.x, c.y); }
void Canvas::clip_rect(const Rect& r) { sk(c_)->clipRect(to_sk(r), true); }
void Canvas::clip_rrect(const Rect& r, float radius) {
    sk(c_)->clipRRect(SkRRect::MakeRectXY(to_sk(r), radius, radius), true);
}

namespace {
SkRRect make_rrect(const Rect& r, const CornerRadii& c) {
    SkVector radii[4] = {{c.tl, c.tl}, {c.tr, c.tr}, {c.br, c.br}, {c.bl, c.bl}};
    SkRRect rr;
    rr.setRectRadii(to_sk(r), radii);
    return rr;
}
} // namespace

void Canvas::clip_rrect(const Rect& r, const CornerRadii& radii) { sk(c_)->clipRRect(make_rrect(r, radii), true); }
void Canvas::draw_rrect(const Rect& r, const CornerRadii& radii, const Paint& p) {
    sk(c_)->drawRRect(make_rrect(r, radii), to_sk_paint(p));
}

void Canvas::clear(Color c) { sk(c_)->clear(to_sk(c)); }
void Canvas::draw_rect(const Rect& r, const Paint& p) { sk(c_)->drawRect(to_sk(r), to_sk_paint(p)); }
void Canvas::draw_rrect(const Rect& r, float radius, const Paint& p) {
    sk(c_)->drawRoundRect(to_sk(r), radius, radius, to_sk_paint(p));
}
void Canvas::draw_circle(Point c, float radius, const Paint& p) {
    sk(c_)->drawCircle(c.x, c.y, radius, to_sk_paint(p));
}
void Canvas::draw_line(Point a, Point b, const Paint& p) {
    SkPaint sp = to_sk_paint(p);
    sp.setStyle(SkPaint::kStroke_Style);
    sk(c_)->drawLine(a.x, a.y, b.x, b.y, sp);
}
void Canvas::draw_path(const Path& path, const Paint& p) {
    sk(c_)->drawPath(path.impl_->builder.snapshot(), to_sk_paint(p));
}

namespace {

// Shadow cache. Blurring is expensive, so a shadow is rendered once per device size and kept as
// four edge strips (top/bottom full width, left/right between them) that are blitted 1:1 every
// frame: no per-frame blur, no image stretching (slow on the CPU raster pipeline), and the
// interior is a plain solid rect (or skipped entirely for Hollow shadows under opaque shapes).
struct ShadowStrips {
    sk_sp<SkImage> top, bottom, left, right;
    SkIRect inner;          // interior not covered by the strips, relative to the shadow bounds
};

struct ShadowKey {
    int w, h;                        // shadow bounds (device px)
    SkIRect shape;                   // blurred shape, relative to the bounds
    int radius_q, sigma_q;           // quantized to 1/4 device px
    int inner_margin;
    SkColor color;
    bool operator==(const ShadowKey&) const = default;
};

sk_sp<SkImage> render_strip(const SkIRect& strip, const SkRRect& shape, float sigma, SkColor color) {
    if (strip.isEmpty()) return nullptr;
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(strip.width(), strip.height()));
    if (!surface) return nullptr;
    SkCanvas* c = surface->getCanvas();
    c->clear(SK_ColorTRANSPARENT);
    c->translate(-strip.x(), -strip.y());
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(color);
    if (sigma > 0) p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));
    c->drawRRect(shape, p);
    return surface->makeImageSnapshot();
}

const ShadowStrips& shadow_strips(const ShadowKey& key, float radius, float sigma) {
    static std::vector<std::pair<ShadowKey, ShadowStrips>> cache;   // small MRU list
    for (size_t i = 0; i < cache.size(); ++i)
        if (cache[i].first == key) {
            if (i) std::swap(cache[i], cache[0]);
            return cache[0].second;
        }
    SkRRect shape = SkRRect::MakeRectXY(SkRect::Make(key.shape), radius, radius);
    // Strips cover everything outside `inner` = shape inset by inner_margin, where the
    // shadow is fully opaque (and, for window shadows, hidden under the window).
    SkIRect inner = key.shape.makeInset(key.inner_margin, key.inner_margin);
    if (inner.isEmpty()) inner = SkIRect::MakeXYWH((key.shape.left() + key.shape.right()) / 2, (key.shape.top() + key.shape.bottom()) / 2, 0, 0);
    ShadowStrips st;
    st.inner = inner;
    st.top    = render_strip(SkIRect::MakeLTRB(0, 0, key.w, inner.top()), shape, sigma, key.color);
    st.bottom = render_strip(SkIRect::MakeLTRB(0, inner.bottom(), key.w, key.h), shape, sigma, key.color);
    st.left   = render_strip(SkIRect::MakeLTRB(0, inner.top(), inner.left(), inner.bottom()), shape, sigma, key.color);
    st.right  = render_strip(SkIRect::MakeLTRB(inner.right(), inner.top(), key.w, inner.bottom()), shape, sigma, key.color);
    cache.insert(cache.begin(), {key, std::move(st)});
    if (cache.size() > 8) cache.pop_back();   // e.g. 2 layers x active/inactive x a few sizes
    return cache[0].second;
}

} // namespace

void Canvas::draw_shadow(const Rect& r, float radius, Color color, float blur_sigma, Point offset,
                         float spread, ShadowMode mode, float opacity) {
    opacity = std::clamp(opacity, 0.f, 1.f);
    if (color.a == 0 || opacity <= 0) return;
    SkPaint fade_paint;   // applied to the cached strips (none at full opacity: plain blit)
    fade_paint.setAlphaf(opacity);
    const SkPaint* fade = opacity < 1 ? &fade_paint : nullptr;
    SkCanvas* c = sk(c_);
    SkRect rect = to_sk(r).makeOffset(offset.x, offset.y).makeOutset(spread, spread);
    float rad = std::max(0.f, radius + spread);

    const SkMatrix m = c->getTotalMatrix();
    float s = m.getScaleX();
    if (m.isScaleTranslate() && s > 0 && std::fabs(s - m.getScaleY()) < 1e-4f) {
        // Device space, integer aligned: the cached strips are blitted 1:1.
        int ext = (int)std::ceil(3 * blur_sigma * s);
        SkIRect shape_dev = m.mapRect(rect).round();
        SkIRect bounds = shape_dev.makeOutset(ext, ext);
        if (bounds.width() <= 8192 && bounds.height() <= 8192) {
            float rr = rad * s, sig = blur_sigma * s;
            // Inside this margin the shadow is solid: past the corner curvature, the blur reach,
            // and the shadow's own offset (so a window drawn at r covers the rest).
            int off = (int)std::ceil(std::max(std::fabs(offset.x), std::fabs(offset.y)) * s);
            int margin = (int)std::ceil(rr) + ext + off + 1;
            SkIRect shape_rel = shape_dev.makeOffset(-bounds.x(), -bounds.y());
            const int W = bounds.width(), H = bounds.height();
            // Between the corners every strip is constant along its length, so one strip
            // rendered at a bucketed length serves every size in that bucket: it is drawn in two
            // halves (left corner + right corner), meeting in the constant middle. A window being
            // resized then re-blurs only when it crosses a 256 px bucket, not on every step.
            const int lead_x = shape_rel.left() + margin, trail_x = W - shape_rel.right() + margin;
            const int lead_y = shape_rel.top() + margin, trail_y = H - shape_rel.bottom() + margin;
            // Once a size has been stable for a few frames, switch to an exact-size strip so the
            // steady state is a single blit per strip again.
            // Tracked per shadow style (a window draws several layers with different bounds).
            struct Stability { int radius_q, sigma_q; SkColor color; int w, h, frames; };
            static std::vector<Stability> stability;
            int rq = (int)std::lround(rr * 4), sq = (int)std::lround(sig * 4);
            SkColor col = to_sk(color);
            Stability* stab = nullptr;
            for (auto& e : stability) if (e.radius_q == rq && e.sigma_q == sq && e.color == col) stab = &e;
            if (!stab) {
                if (stability.size() >= 16) stability.erase(stability.begin());
                stability.push_back({rq, sq, col, 0, 0, 0});
                stab = &stability.back();
            }
            stab->frames = (stab->w == W && stab->h == H) ? stab->frames + 1 : 0;
            stab->w = W; stab->h = H;
            auto bucket = [](int v) { return (v + 255) / 256 * 256; };
            bool resizing = stab->frames < 3;
            int Wc = (resizing && W / 2 >= lead_x && W - W / 2 >= trail_x) ? bucket(W) : W;
            int Hc = (resizing && H / 2 >= lead_y && H - H / 2 >= trail_y) ? bucket(H) : H;
            SkIRect shape_c = SkIRect::MakeLTRB(shape_rel.left(), shape_rel.top(),
                                                Wc - (W - shape_rel.right()), Hc - (H - shape_rel.bottom()));
            ShadowKey key{Wc, Hc, shape_c, (int)std::lround(rr * 4), (int)std::lround(sig * 4), margin, to_sk(color)};
            const ShadowStrips& st = shadow_strips(key, rr, sig);
            // Inner rect in real (not bucketed) coordinates.
            SkIRect inner = SkIRect::MakeLTRB(st.inner.left(), st.inner.top(),
                                              W - (Wc - st.inner.right()), H - (Hc - st.inner.bottom()));
            c->save();
            c->resetMatrix();
            c->translate((float)bounds.x(), (float)bounds.y());
            SkSamplingOptions nearest;
            // Blit `img` (cache length `src_len`) at real length `len`, split in the middle.
            // Each half is a clipped 1:1 drawImage (a plain blit; src-rect draws are much slower
            // on the raster backend).
            auto blit_h = [&](const sk_sp<SkImage>& img, int y, int len) {
                if (!img) return;
                int h = img->height(), half = len / 2, src_len = img->width();
                if (src_len == len) { c->drawImage(img, 0, (float)y, nearest, fade); return; }
                c->save(); c->clipIRect(SkIRect::MakeXYWH(0, y, half, h));
                c->drawImage(img, 0, (float)y, nearest, fade); c->restore();
                c->save(); c->clipIRect(SkIRect::MakeXYWH(half, y, len - half, h));
                c->drawImage(img, (float)(len - src_len), (float)y, nearest, fade); c->restore();
            };
            auto blit_v = [&](const sk_sp<SkImage>& img, int x, int len) {
                if (!img) return;
                int w = img->width(), half = len / 2, src_len = img->height(), y0 = inner.top();
                if (src_len == len) { c->drawImage(img, (float)x, (float)y0, nearest, fade); return; }
                c->save(); c->clipIRect(SkIRect::MakeXYWH(x, y0, w, half));
                c->drawImage(img, (float)x, (float)y0, nearest, fade); c->restore();
                c->save(); c->clipIRect(SkIRect::MakeXYWH(x, y0 + half, w, len - half));
                c->drawImage(img, (float)x, (float)(y0 + len - src_len), nearest, fade); c->restore();
            };
            blit_h(st.top, 0, W);
            blit_h(st.bottom, inner.bottom(), W);
            blit_v(st.left, 0, inner.height());
            blit_v(st.right, inner.right(), inner.height());
            if (mode == ShadowMode::Normal && !inner.isEmpty()) {
                SkPaint solid;
                solid.setColor(to_sk(color));
                solid.setAlphaf(solid.getAlphaf() * opacity);
                c->drawIRect(inner, solid);
            }
            c->restore();
            return;
        }
    }
    // General path (rotation/skew, huge shapes): blur directly.
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(to_sk(color));
    paint.setAlphaf(paint.getAlphaf() * opacity);
    if (blur_sigma > 0) paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur_sigma));
    c->drawRRect(SkRRect::MakeRectXY(rect, rad, rad), paint);
}

// draw_text(const TextLayout&, Point) lives in text_skia.cpp, draw_image in image_skia.cpp.

Size Canvas::draw_text(std::string_view utf8, Point top_left, const TextStyle& style) {
    TextLayout layout(utf8, style);
    draw_text(layout, top_left);
    return layout.size();
}

} // namespace uitk
