#include "uitk/widgets/button.h"

#include <algorithm>
#include <cmath>

#include "uitk/widgets/label.h"

namespace uitk {

namespace {
constexpr uint32_t kKpEnter = 0xff8d, kSpace = 0x20;
}

ButtonLook button_look(ButtonStyle style, const Theme& t, bool icon_only) {
    ButtonLook l;
    l.radius = t.radius;
    l.height = t.control_height;
    l.padding_x = t.control_padding_x;
    l.icon_only_width = t.control_height;
    switch (style) {
        case ButtonStyle::Secondary:   l.colors = t.button; break;
        case ButtonStyle::Primary:     l.colors = t.button_primary; break;
        case ButtonStyle::Destructive: l.colors = t.button_destructive; break;
        case ButtonStyle::Flat:
            l.colors = t.button_flat;
            if (icon_only) {   // toolbar action item
                l.radius = t.toolbar_radius;
                l.height = l.icon_only_width = t.toolbar_button_size;
            }
            break;
        case ButtonStyle::Circular:
            l.colors = t.button;
            l.circle = true;
            break;
        case ButtonStyle::WindowControl:
            l.colors = t.window_control;
            if (t.window_controls == WindowControlsStyle::Circular) {
                l.circle = true;
                l.height = l.icon_only_width = t.window_button_height;
            } else {
                l.radius = 0;   // flat, full title-bar height
                l.height = t.window_button_height;
                l.icon_only_width = t.window_button_width;
            }
            break;
        case ButtonStyle::Link:
            l.colors = {Color::transparent(), t.link, Color::transparent()};
            l.padding_x = 0;
            l.height = std::ceil(t.font_size * 1.5f);
            break;
        case ButtonStyle::Tab:
            l.colors = {Color::transparent(), t.chrome_fg, t.button_flat.overlay};
            l.radius = 0;
            l.height = t.tab_height;
            l.padding_x = 14;
            break;
        case ButtonStyle::StatusItem:
            l.colors = {Color::transparent(), t.chrome_fg, t.button_flat.overlay};
            l.radius = 0;
            l.height = t.status_bar_height;
            l.padding_x = 6;
            l.icon_only_width = t.status_bar_height + 4;
            l.text_role = TextRole::Caption;
            break;
    }
    return l;
}

Button::Button(std::string label, ButtonStyle style) : label_(std::move(label)), style_(style) {
    set_align(Align::Start, Align::Center);
}

Button::Button(Image icon, ButtonStyle style) : icon_(std::move(icon)), style_(style) {
    set_align(Align::Start, Align::Center);
}

void Button::set_label(std::string label) {
    if (label == label_) return;
    label_ = std::move(label);
    invalidate_layout();
}

void Button::set_icon(Image icon) {
    icon_ = std::move(icon);
    invalidate_layout();
}

float Button::icon_px(const Theme& t) const {
    if (icon_size_ > 0) return icon_size_;
    return style_ == ButtonStyle::WindowControl ? t.window_icon_size : t.icon_size;
}

CornerRadii Button::radii(const ButtonLook& l) const {
    if (corners_) return *corners_;
    return CornerRadii::all(l.radius);
}

// ---------------------------------------------------------------------------- state

uint32_t Button::visual_state() const {
    return state() | (key_pressed_ ? STATE_PRESSED : 0u);
}

void Button::on_state_changed(uint32_t) {
    if (!(state() & STATE_FOCUSED)) key_pressed_ = false;
    layer_.update(visual_state(), theme());
}

// ---------------------------------------------------------------------------- layout / paint

Size Button::measure(float, const Theme& t) {
    bool has_icon = icon_.valid(), has_text = !label_.empty();
    ButtonLook l = look(t);
    if (l.circle) return {l.height, l.height};
    if (has_icon && !has_text) return {l.icon_only_width, l.height};
    float w = 2 * l.padding_x;
    if (has_icon) w += icon_px(t);
    if (has_icon && has_text) w += t.spacing;
    if (has_text) w += std::ceil(TextLayout(label_, Label::style_for(t, l.text_role, TextAlign::Left, true)).size().width);
    return {w, l.height};
}

void Button::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    ButtonLook l = look(t);
    StateLayer::Sample s = layer_.sample(ctx.clock);
    Rect r = local_rect();

    Color fill = StateLayer::surface(l.colors.background, l.colors.overlay, s, t);
    if (fill.a) {
        if (l.circle) ctx.canvas.draw_circle(r.center(), std::min(r.width, r.height) / 2, Paint::fill(fill));
        else ctx.canvas.draw_rrect(r, radii(l), Paint::fill(fill));
    }

    Color fg = StateLayer::content(backdrop_dim_ ? ctx.backdrop(l.colors.foreground) : l.colors.foreground, s, t);
    bool has_icon = icon_.valid(), has_text = !label_.empty();
    float ip = icon_px(t);
    TextLayout text;
    float content_w = 0;
    if (has_icon) content_w += ip;
    if (has_text) {
        TextStyle ts = Label::style_for(t, l.text_role, TextAlign::Left, true);
        ts.color = fg;
        float max_text = std::max(0.f, r.width - 2 * l.padding_x - (has_icon ? ip + t.spacing : 0));
        text = TextLayout(label_, ts, max_text);
        content_w += (has_icon ? t.spacing : 0) + text.size().width;
    }
    // Content is centred as a group; positions snap to whole pixels for crisp glyphs and icons.
    float x = std::round((r.width - content_w) / 2);
    if (has_icon) {
        ctx.canvas.draw_image(icon_, {x, std::round((r.height - ip) / 2), ip, ip}, fg);
        x += ip + (has_text ? t.spacing : 0);
    }
    if (has_text) {
        float ty = std::round((r.height - text.size().height) / 2);
        ctx.canvas.draw_text(text, {x, ty});
        if (style_ == ButtonStyle::Link && s.hover > 0) {   // underline fades in on hover
            float uy = ty + text.size().height - 1.5f;
            ctx.canvas.draw_line({x, uy}, {x + text.size().width, uy}, Paint::stroke(fg.faded(s.hover), 1));
        }
    }
}

void Button::paint_over(PaintContext& ctx) {
    StateLayer::Sample s = layer_.sample(ctx.clock);
    ButtonLook l = look(ctx.theme);
    float radius = l.circle ? size().height / 2 : radii(l).tl;
    StateLayer::paint_focus_ring(ctx.canvas, local_rect(), radius, s, ctx.theme);
}

// ---------------------------------------------------------------------------- input

bool Button::on_pointer(const PointerEvent& e) {
    if (e.button != PointerButton::Left) return e.type == PointerEvent::Type::Down;   // swallow presses
    if (e.type == PointerEvent::Type::Up && local_rect().contains(e.pos) && enabled()) clicked.emit();
    return e.type == PointerEvent::Type::Down || e.type == PointerEvent::Type::Up;
}

bool Button::on_key(const KeyEvent& e) {
    if (!enabled()) return false;
    if (e.keysym == kSpace) {
        if (e.pressed && !e.repeat) { key_pressed_ = true; layer_.update(visual_state(), theme()); invalidate(); }
        else if (!e.pressed && key_pressed_) {
            key_pressed_ = false;
            layer_.update(visual_state(), theme());
            invalidate();
            clicked.emit();
        }
        return true;
    }
    if ((e.keysym == key::Return || e.keysym == kKpEnter) && e.pressed) {
        if (!e.repeat) clicked.emit();
        return true;
    }
    return false;
}

} // namespace uitk
