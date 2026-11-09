// Text: skparagraph (HarfBuzz shaping + ICU breaking) over the platform font manager
// (fontconfig + FreeType on Linux, DirectWrite on Windows).
#include "skia_internal.h"

#include <cstdio>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>

#include "include/core/SkFontStyle.h"
#ifdef _WIN32
#include "include/ports/SkTypeface_win.h"
#else
#include "include/ports/SkFontMgr_fontconfig.h"
#include "include/ports/SkFontScanner_FreeType.h"
#endif
#include "include/core/SkBlurTypes.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkTextBlob.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphPainter.h"
#include "modules/skparagraph/include/ParagraphStyle.h"
#include "modules/skparagraph/include/TextStyle.h"
#include "modules/skunicode/include/SkUnicode_icu.h"

namespace uitk {

namespace tl = ::skia::textlayout;

namespace skglue {

sk_sp<SkFontMgr> font_manager() {
#ifdef _WIN32
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_DirectWrite();
#else
    static sk_sp<SkFontMgr> mgr = SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
#endif
    return mgr;
}

sk_sp<tl::FontCollection> font_collection() {
    static sk_sp<tl::FontCollection> fc = [] {
        auto c = sk_make_sp<tl::FontCollection>();
        c->setDefaultFontManager(font_manager());
        c->enableFontFallback();
        return c;
    }();
    return fc;
}

sk_sp<SkUnicode> unicode() {
    static sk_sp<SkUnicode> u = SkUnicodes::ICU::Make();
    return u;
}

} // namespace skglue

struct TextLayout::Impl {
    std::unique_ptr<tl::Paragraph> paragraph;
    std::string text;
    TextStyle   style;
    float       width = 0;
};

namespace {

std::shared_ptr<TextLayout::Impl> build_layout(std::string_view utf8, const TextStyle& style, float max_width) {
    tl::TextStyle ts;
    std::vector<SkString> families;
    for (auto& f : style.families) families.emplace_back(f.c_str());
    ts.setFontFamilies(families);
    ts.setFontSize(style.size);
    ts.setColor(skglue::to_sk(style.color));
    ts.setFontStyle(SkFontStyle(style.weight, SkFontStyle::kNormal_Width,
                                style.italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant));

    tl::ParagraphStyle ps;
    ps.setTextStyle(ts);
    switch (style.align) {
        case TextAlign::Left:    ps.setTextAlign(tl::TextAlign::kLeft); break;
        case TextAlign::Center:  ps.setTextAlign(tl::TextAlign::kCenter); break;
        case TextAlign::Right:   ps.setTextAlign(tl::TextAlign::kRight); break;
        case TextAlign::Justify: ps.setTextAlign(tl::TextAlign::kJustify); break;
    }
    if (style.max_lines > 0) ps.setMaxLines((size_t)style.max_lines);
    if (style.ellipsis) ps.setEllipsis(u"\u2026");

    auto builder = tl::ParagraphBuilder::make(ps, skglue::font_collection(), skglue::unicode());
    builder->pushStyle(ts);
    builder->addText(utf8.data(), utf8.size());
    auto impl = std::make_shared<TextLayout::Impl>();
    impl->paragraph = builder->Build();
    impl->paragraph->layout(max_width);
    impl->text = std::string(utf8);
    impl->style = style;
    impl->width = max_width;
    return impl;
}

// Shaping (HarfBuzz + ICU + font fallback) dominates text cost, and UIs redraw the same
// strings every frame. Layouts are immutable once cached, so they are shared freely.
class LayoutCache {
public:
    std::shared_ptr<TextLayout::Impl> get(std::string_view utf8, const TextStyle& st, float width) {
        std::string key;
        key.reserve(utf8.size() + 96);
        for (auto& f : st.families) { key += f; key += ','; }
        char buf[96];
        // Colour is deliberately not part of the key: it is applied at draw time.
        snprintf(buf, sizeof buf, "|%g|%d|%d|%d|%d|%d|%g|", st.size, st.weight, (int)st.italic,
                 (int)st.align, st.max_lines, (int)st.ellipsis, width);
        key += buf;
        key += utf8;
        auto it = map_.find(key);
        if (it != map_.end()) {
            lru_.splice(lru_.begin(), lru_, it->second);   // mark most recently used
            return it->second->second;
        }
        auto impl = build_layout(utf8, st, width);
        lru_.emplace_front(key, impl);
        map_[std::move(key)] = lru_.begin();
        if (lru_.size() > kCapacity) {
            map_.erase(lru_.back().first);
            lru_.pop_back();
        }
        return impl;
    }

private:
    static constexpr size_t kCapacity = 512;
    using Entry = std::pair<std::string, std::shared_ptr<TextLayout::Impl>>;
    std::list<Entry> lru_;
    std::unordered_map<std::string, std::list<Entry>::iterator> map_;
};

LayoutCache& layout_cache() {
    static LayoutCache cache;
    return cache;
}

} // namespace

TextLayout::TextLayout(std::string_view utf8, const TextStyle& style, float max_width)
    : impl_(layout_cache().get(utf8, style, max_width)), color_(style.color) {}

void TextLayout::layout(float max_width) {
    // Cached layouts are shared, so never re-wrap in place: fetch/build the layout for the
    // new width instead (copy-on-write semantics).
    if (impl_ && impl_->width != max_width)
        impl_ = layout_cache().get(impl_->text, impl_->style, max_width);
}

namespace {
// UTF-8 byte offset <-> UTF-16 code unit index (skparagraph positions are UTF-16).
size_t utf16_index(const std::string& s, size_t byte) {
    size_t u16 = 0;
    for (size_t i = 0; i < s.size() && i < byte;) {
        unsigned char c = (unsigned char)s[i];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        u16 += len == 4 ? 2 : 1;
        i += len;
    }
    return u16;
}
size_t byte_index(const std::string& s, size_t u16) {
    size_t i = 0, n = 0;
    while (i < s.size() && n < u16) {
        unsigned char c = (unsigned char)s[i];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        n += len == 4 ? 2 : 1;
        i += len;
    }
    return std::min(i, s.size());
}
} // namespace

const std::string& TextLayout::text() const {
    static const std::string empty;
    return impl_ ? impl_->text : empty;
}

float TextLayout::caret_x(size_t byte) const {
    if (!impl_ || byte == 0) return 0;
    auto boxes = impl_->paragraph->getRectsForRange(0, (unsigned)utf16_index(impl_->text, byte),
                                                    tl::RectHeightStyle::kMax, tl::RectWidthStyle::kTight);
    float x = 0;
    for (auto& b : boxes) x = std::max(x, b.rect.fRight);
    return x;
}

size_t TextLayout::index_at(float x, float y) const {
    if (!impl_) return 0;
    auto pos = impl_->paragraph->getGlyphPositionAtCoordinate(x, y);
    return byte_index(impl_->text, (size_t)std::max(0, pos.position));
}

std::vector<Rect> TextLayout::range_rects(size_t from, size_t to) const {
    std::vector<Rect> out;
    if (!impl_ || from >= to) return out;
    auto boxes = impl_->paragraph->getRectsForRange((unsigned)utf16_index(impl_->text, from),
                                                    (unsigned)utf16_index(impl_->text, to),
                                                    tl::RectHeightStyle::kMax, tl::RectWidthStyle::kTight);
    for (auto& b : boxes) out.push_back(Rect::from_ltrb(b.rect.fLeft, b.rect.fTop, b.rect.fRight, b.rect.fBottom));
    return out;
}

Size TextLayout::size() const {
    if (!impl_) return {};
    return {impl_->paragraph->getLongestLine(), impl_->paragraph->getHeight()};
}

float TextLayout::max_intrinsic_width() const {
    return impl_ ? impl_->paragraph->getMaxIntrinsicWidth() : 0.f;
}

namespace {

// Paints a (shared, cached) paragraph with a colour chosen at draw time. The paragraph itself
// is never modified, so one shaped layout serves any colour (theme cross-fades, hover states).
class RecolorPainter final : public tl::ParagraphPainter {
public:
    RecolorPainter(SkCanvas* canvas, SkColor color) : c_(canvas), color_(color) {}

    void drawTextBlob(const sk_sp<SkTextBlob>& blob, SkScalar x, SkScalar y, const SkPaintOrID& paint) override {
        SkPaint p = std::holds_alternative<SkPaint>(paint) ? std::get<SkPaint>(paint) : SkPaint();
        p.setColor(color_);
        c_->drawTextBlob(blob, x, y, p);
    }
    void drawTextShadow(const sk_sp<SkTextBlob>& blob, SkScalar x, SkScalar y, SkColor color, SkScalar sigma) override {
        SkPaint p;
        p.setColor(color);
        if (sigma > 0) p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));
        c_->drawTextBlob(blob, x, y, p);
    }
    void drawRect(const SkRect& rect, const SkPaintOrID& paint) override {   // text backgrounds
        if (std::holds_alternative<SkPaint>(paint)) c_->drawRect(rect, std::get<SkPaint>(paint));
    }
    void drawFilledRect(const SkRect& rect, const DecorationStyle& d) override { c_->drawRect(rect, decor(d)); }
    void drawPath(const SkPath& path, const DecorationStyle& d) override { c_->drawPath(path, decor(d)); }
    void drawLine(SkScalar x0, SkScalar y0, SkScalar x1, SkScalar y1, const DecorationStyle& d) override {
        c_->drawLine(x0, y0, x1, y1, decor(d));
    }
    void clipRect(const SkRect& rect) override { c_->clipRect(rect); }
    void translate(SkScalar dx, SkScalar dy) override { c_->translate(dx, dy); }
    void save() override { c_->save(); }
    void restore() override { c_->restore(); }

private:
    SkPaint decor(const DecorationStyle& d) const {   // underline etc. follow the text colour
        SkPaint p = d.skPaint();
        p.setColor(color_);
        return p;
    }
    SkCanvas* c_;
    SkColor   color_;
};

} // namespace

void Canvas::draw_text(const TextLayout& text, Point p) {
    if (!text.impl_) return;
    RecolorPainter painter(skglue::sk(c_), skglue::to_sk(text.color_));
    text.impl_->paragraph->paint(&painter, p.x, p.y);
}

} // namespace uitk
