#include "uitk/widgets/range.h"

#include <algorithm>
#include <cmath>

namespace uitk {

// ---------------------------------------------------------------------------- Slider

Slider::Slider(float min, float max, float value, float step) : min_(min), max_(max), value_(min), step_(step) {
    set_align(Align::Fill, Align::Center);
    set_value(value);
}

void Slider::set_range(float min, float max) {
    min_ = min; max_ = std::max(min, max);
    set_value(value_);
}

void Slider::set_value(float v) {
    v = std::clamp(v, min_, max_);
    if (step_ > 0) v = std::clamp(min_ + std::round((v - min_) / step_) * step_, min_, max_);
    if (v == value_) return;
    value_ = v;
    invalidate();
    changed.emit(value_);
}

Size Slider::measure(float, const Theme& t) { return {180, std::max(t.control_height, t.slider_thumb + 8)}; }

Rect Slider::track_rect(const Theme& t) const {
    float r = t.slider_thumb / 2;   // the thumb stays fully inside at both ends
    return {r, std::round((size().height - t.track_thickness) / 2), std::max(0.f, size().width - 2 * r), t.track_thickness};
}

void Slider::set_from_x(float x) {
    Rect tr = track_rect(theme());
    float f = tr.width > 0 ? std::clamp((x - tr.x) / tr.width, 0.f, 1.f) : 0;
    set_value(min_ + f * (max_ - min_));
}

void Slider::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    StateLayer::Sample s = layer_.sample(ctx.clock);
    Rect tr = track_rect(t);
    float r = t.track_thickness / 2;
    float x = tr.x + tr.width * fraction();
    ctx.canvas.draw_rrect(tr, r, Paint::fill(StateLayer::content(t.track, s, t)));
    ctx.canvas.draw_rrect({tr.x, tr.y, x - tr.x, tr.height}, r, Paint::fill(StateLayer::content(t.check_fill, s, t)));
    Point c{x, tr.y + tr.height / 2};
    // Hover/press halo, then the thumb.
    float halo = (s.hover * 0.6f + s.press * 0.4f) * (1 - s.disabled);
    if (halo > 0) ctx.canvas.draw_circle(c, t.slider_thumb / 2 + 4, Paint::fill(t.check_fill.with_opacity(0.25f * halo)));
    ctx.canvas.draw_circle(c, t.slider_thumb / 2, Paint::fill(StateLayer::content(t.check_fill, s, t)));
}

void Slider::paint_over(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    Rect tr = track_rect(t);
    float x = tr.x + tr.width * fraction(), r = t.slider_thumb / 2;
    StateLayer::paint_focus_ring(ctx.canvas, {x - r, tr.y + tr.height / 2 - r, 2 * r, 2 * r}, r, layer_.sample(ctx.clock), t);
}

bool Slider::on_pointer(const PointerEvent& e) {
    if (e.button != PointerButton::Left && e.type != PointerEvent::Type::Move) return e.type == PointerEvent::Type::Down;
    switch (e.type) {
        case PointerEvent::Type::Down: dragging_ = true; set_from_x(e.pos.x); on_state_changed(state()); return true;
        case PointerEvent::Type::Move: if (dragging_) set_from_x(e.pos.x); return dragging_;
        case PointerEvent::Type::Up:   dragging_ = false; on_state_changed(state()); return true;
        default: return false;
    }
}

bool Slider::on_key(const KeyEvent& e) {
    if (!e.pressed || !enabled()) return false;
    float step = step_ > 0 ? step_ : (max_ - min_) / 100;
    switch (e.keysym) {
        case key::Left: case key::Down: set_value(value_ - step); return true;
        case key::Right: case key::Up:  set_value(value_ + step); return true;
        case key::PageDown:             set_value(value_ - 10 * step); return true;
        case key::PageUp:               set_value(value_ + 10 * step); return true;
        case key::Home:                 set_value(min_); return true;
        case key::End:                  set_value(max_); return true;
    }
    return false;
}

// ---------------------------------------------------------------------------- ProgressBar

ProgressBar::ProgressBar(float fraction) {
    set_align(Align::Fill, Align::Center);
    set_fraction(fraction);
    shown_.jump(target_);
}

void ProgressBar::set_fraction(float f) {
    target_ = std::clamp(f, 0.f, 1.f);
    shown_.set(target_, {Millis(200), Easing::ease_out_quad()});
    invalidate();
}

void ProgressBar::set_indeterminate(bool on) {
    if (on) loop_.start(); else loop_.stop();
    invalidate();
}

Size ProgressBar::measure(float, const Theme& t) { return {180, t.track_thickness}; }

void ProgressBar::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    Rect r = local_rect();
    float rad = r.height / 2;
    ctx.canvas.draw_rrect(r, rad, Paint::fill(t.track));
    if (loop_.running()) {
        // A segment sweeping across, eased so it accelerates in and out.
        float p = Easing::ease_in_out()(loop_.phase(ctx.clock));
        float seg = r.width * 0.3f;
        float x = -seg + p * (r.width + seg);
        ctx.canvas.save();
        ctx.canvas.clip_rrect(r, rad);
        ctx.canvas.draw_rect({x, r.y, seg, r.height}, Paint::fill(t.check_fill));
        ctx.canvas.restore();
    } else {
        float w = r.width * shown_.value(ctx.clock);
        if (w > 0) ctx.canvas.draw_rrect({r.x, r.y, w, r.height}, rad, Paint::fill(t.check_fill));
    }
}

// ---------------------------------------------------------------------------- Spinner

Spinner::Spinner(float size) : size_(size) { set_align(Align::Start, Align::Center); }

Size Spinner::measure(float, const Theme& t) {
    float s = size_ > 0 ? size_ : t.icon_size;
    return {s, s};
}

void Spinner::paint(PaintContext& ctx) {
    if (!loop_.running()) return;
    const Theme& t = ctx.theme;
    float s = size_ > 0 ? size_ : t.icon_size;
    float stroke = std::max(1.5f, s / 9);
    Rect oval = Rect{(size().width - s) / 2, (size().height - s) / 2, s, s}.inset(stroke / 2, stroke / 2);
    ctx.canvas.draw_circle(oval.center(), oval.width / 2, Paint::stroke(t.track, stroke));
    Path arc;
    arc.add_arc(oval, loop_.phase(ctx.clock) * 360.f - 90.f, 100.f);
    ctx.canvas.draw_path(arc, Paint::stroke(t.check_fill, stroke));
}

} // namespace uitk
