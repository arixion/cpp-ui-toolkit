#include "uitk/widgets/toggles.h"

#include <algorithm>
#include <cmath>

#include "uitk/widgets/glyphs.h"

namespace uitk {

namespace {
constexpr uint32_t kSpace = 0x20, kKpEnter = 0xff8d;
constexpr float kLabelGap = 8;
}

// ---------------------------------------------------------------------------- ToggleControl

ToggleControl::ToggleControl(std::string label) {
    set_align(Align::Start, Align::Center);
    label_ = &add<Label>(std::move(label));
    label_->set_visible(!label_->text().empty());
}

void ToggleControl::set_checked(bool c) {
    if (c == checked()) return;
    set_checked_state(c);
    toggled.emit(c);
}

void ToggleControl::on_state_changed(uint32_t) { layer_.update(state(), theme()); }

Size ToggleControl::measure(float available_width, const Theme& t) {
    Size ind = indicator_size(t);
    float w = ind.width, h = std::max(ind.height, std::ceil(t.font_size * 1.6f));
    if (label_->visible()) {
        Size ls = label_->measure_clamped(std::max(0.f, available_width - ind.width - kLabelGap), t);
        w += kLabelGap + ls.width;
        h = std::max(h, ls.height);
    }
    return {w, h};
}

void ToggleControl::layout(const Theme& t) {
    Size ind = indicator_size(t);
    float h = size().height;
    indicator_ = {0, std::round((h - ind.height) / 2), ind.width, ind.height};
    if (label_->visible()) {
        float x = ind.width + kLabelGap;
        label_->set_bounds({x, 0, std::max(0.f, size().width - x), h});
        label_->layout(t);
    }
}

void ToggleControl::paint(PaintContext& ctx) { paint_indicator(ctx, indicator_, layer_.sample(ctx.clock)); }

void ToggleControl::paint_over(PaintContext& ctx) {
    StateLayer::paint_focus_ring(ctx.canvas, indicator_, indicator_radius(ctx.theme), layer_.sample(ctx.clock), ctx.theme);
}

bool ToggleControl::on_pointer(const PointerEvent& e) {
    if (e.button != PointerButton::Left) return e.type == PointerEvent::Type::Down;
    if (e.type == PointerEvent::Type::Up && local_rect().contains(e.pos) && enabled()) activate();
    return true;
}

bool ToggleControl::on_key(const KeyEvent& e) {
    if (!enabled()) return false;
    if ((e.keysym == kSpace && !e.pressed) || ((e.keysym == key::Return || e.keysym == kKpEnter) && e.pressed && !e.repeat)) {
        activate();
        return true;
    }
    return e.keysym == kSpace;   // swallow the Space press too
}

// ---------------------------------------------------------------------------- CheckBox

void CheckBox::paint_indicator(PaintContext& ctx, const Rect& r, const StateLayer::Sample& s) {
    const Theme& t = ctx.theme;
    float on = indeterminate_ ? 1.f : s.checked;
    float radius = t.radius - 1;
    // Unchecked: field colours + outline; checked: accent fill. Cross-fades with the animation.
    Color base = lerp(t.control_bg, t.check_fill, on);
    Color fill = StateLayer::surface(base, on > 0.5f ? t.check_mark : t.foreground, s, t);
    ctx.canvas.draw_rrect(r, radius, Paint::fill(fill));
    Color border = StateLayer::content(lerp(t.control_border, t.check_fill, on), s, t);
    ctx.canvas.draw_rrect(r.inset(0.5f, 0.5f), radius - 0.5f, Paint::stroke(border, 1));
    Color mark = StateLayer::content(t.check_mark, s, t).faded(on);
    if (indeterminate_) {
        float y = std::round(r.center().y) + 0.5f;
        ctx.canvas.draw_line({r.x + 4, y}, {r.right() - 4, y}, Paint::stroke(mark, 1.5f));
    } else if (on > 0) {
        ctx.canvas.draw_image(glyph(Glyph::Check), r, mark);
    }
}

// ---------------------------------------------------------------------------- RadioGroup / RadioButton

int RadioGroup::selected() const {
    for (size_t i = 0; i < buttons_.size(); ++i) if (buttons_[i]->checked()) return (int)i;
    return -1;
}

void RadioGroup::select(int index) {
    if (index >= 0 && index < (int)buttons_.size()) select(buttons_[(size_t)index]);
}

void RadioGroup::select(RadioButton* b) {
    if (b->checked()) return;
    for (RadioButton* other : buttons_) if (other != b) other->set_checked(false);
    b->set_checked(true);
    changed.emit(selected());
}

RadioButton::RadioButton(std::string label, RadioGroup& group) : ToggleControl(std::move(label)), group_(&group) {
    group_->buttons_.push_back(this);
}

RadioButton::~RadioButton() {
    auto& v = group_->buttons_;
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

void RadioButton::activate() { group_->select(this); }

void RadioButton::paint_indicator(PaintContext& ctx, const Rect& r, const StateLayer::Sample& s) {
    const Theme& t = ctx.theme;
    Point c = r.center();
    float rad = r.width / 2;
    Color base = lerp(t.control_bg, t.check_fill, s.checked);
    ctx.canvas.draw_circle(c, rad, Paint::fill(StateLayer::surface(base, t.foreground, s, t)));
    Color border = StateLayer::content(lerp(t.control_border, t.check_fill, s.checked), s, t);
    ctx.canvas.draw_circle(c, rad - 0.5f, Paint::stroke(border, 1));
    if (s.checked > 0)   // the dot grows in as it is selected
        ctx.canvas.draw_circle(c, rad * 0.4f * s.checked, Paint::fill(StateLayer::content(t.check_mark, s, t)));
}

// ---------------------------------------------------------------------------- Switch

void Switch::paint_indicator(PaintContext& ctx, const Rect& r, const StateLayer::Sample& s) {
    const Theme& t = ctx.theme;
    float radius = r.height / 2;
    Color track = lerp(t.control_bg, t.check_fill, s.checked);
    ctx.canvas.draw_rrect(r, radius, Paint::fill(StateLayer::surface(track, t.foreground, s, t)));
    Color border = StateLayer::content(lerp(t.control_border, t.check_fill, s.checked), s, t);
    ctx.canvas.draw_rrect(r.inset(0.5f, 0.5f), radius - 0.5f, Paint::stroke(border, 1));
    // The knob slides with the checked animation.
    float knob = radius - 3;
    float x0 = r.x + radius, x1 = r.right() - radius;
    Color knob_color = lerp(t.foreground_dim, t.check_mark, s.checked);
    ctx.canvas.draw_circle({lerp(x0, x1, s.checked), r.center().y}, knob + s.press, Paint::fill(StateLayer::content(knob_color, s, t)));
}

// ---------------------------------------------------------------------------- ToggleButton

ToggleButton::ToggleButton(std::string label, ButtonStyle style) : Button(std::move(label), style) {
    clicked.connect([this] { set_checked(!checked()); });
}

ToggleButton::ToggleButton(Image icon, ButtonStyle style) : Button(std::move(icon), style) {
    clicked.connect([this] { set_checked(!checked()); });
}

void ToggleButton::set_checked(bool c) {
    if (c == checked()) return;
    set_checked_state(c);
    toggled.emit(c);
}

ButtonLook ToggleButton::look(const Theme& t) const {
    ButtonLook l = Button::look(t);
    if (!checked()) return l;
    if (style() == ButtonStyle::Tab) l.colors.foreground = t.foreground;   // active tab: bright text
    else l.colors = t.toggle_on;
    return l;
}

void ToggleButton::paint_over(PaintContext& ctx) {
    Button::paint_over(ctx);
    if (style() == ButtonStyle::Tab || !checked()) return;
    // Checked: accent outline (VS Code "input option" style).
    ButtonLook l = look(ctx.theme);
    ctx.canvas.draw_rrect(local_rect().inset(0.5f, 0.5f), std::max(0.f, l.radius - 0.5f), Paint::stroke(ctx.theme.accent, 1));
}

} // namespace uitk
