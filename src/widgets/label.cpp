#include "uitk/widgets/label.h"

#include <cmath>

namespace uitk {

Label::Label(std::string text, TextRole role) : text_(std::move(text)), role_(role) {}

void Label::set_text(std::string text) {
    if (text == text_) return;
    text_ = std::move(text);
    invalidate_layout();
}

TextStyle Label::style_for(const Theme& theme, TextRole role, TextAlign align, bool single_line) {
    TextStyle s = theme.text(role);
    s.align = align;
    if (single_line) { s.max_lines = 1; s.ellipsis = true; }
    return s;
}

Size Label::measure(float available_width, const Theme& theme) {
    if (text_.empty()) return {0, std::ceil(theme.text(role_).size * 1.2f)};
    TextStyle s = style_for(theme, role_, TextAlign::Left, !wrap_);
    TextLayout t(text_, s, wrap_ ? available_width : 1e9f);
    Size sz = t.size();
    return {std::ceil(sz.width), std::ceil(sz.height)};
}

void Label::paint(PaintContext& ctx) {
    if (text_.empty()) return;
    TextStyle s = style_for(ctx.theme, role_, align_, !wrap_);
    Color c = color_ ? *color_ : token_ ? ctx.theme.color(*token_) : ctx.theme.text_color(role_);
    if (backdrop_dim_) c = ctx.backdrop(c);
    if (state() & STATE_DISABLED) c = c.faded(ctx.theme.disabled_opacity);
    s.color = c;
    // Lay out at our width: wraps, aligns and ellipsizes within the allotted space.
    TextLayout t(text_, s, size().width);
    float y = std::round((size().height - t.size().height) / 2);
    ctx.canvas.draw_text(t, {0, y});
}

} // namespace uitk
