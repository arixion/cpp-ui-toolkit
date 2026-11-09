#include "uitk/widgets/state.h"

namespace uitk {

void StateLayer::update(uint32_t state, const Theme& th) {
    if (!initialized_) { jump(state); return; }
    auto to = [](bool on) { return on ? 1.f : 0.f; };
    bool hovered = state & STATE_HOVERED, pressed = state & STATE_PRESSED;
    hover_.set(to(hovered), hovered ? th.hover_in : th.hover_out);
    press_.set(to(pressed), pressed ? th.press_in : th.press_out);
    focus_.set(to(state & STATE_FOCUS_VISIBLE), th.focus);
    disabled_.set(to(state & STATE_DISABLED), th.focus);
    checked_.set(to(state & STATE_CHECKED), th.press_out);
    state_ = state;
}

void StateLayer::jump(uint32_t state) {
    auto to = [](bool on) { return on ? 1.f : 0.f; };
    hover_.jump(to(state & STATE_HOVERED));
    press_.jump(to(state & STATE_PRESSED));
    focus_.jump(to(state & STATE_FOCUS_VISIBLE));
    disabled_.jump(to(state & STATE_DISABLED));
    checked_.jump(to(state & STATE_CHECKED));
    state_ = state;
    initialized_ = true;
}

StateLayer::Sample StateLayer::sample(FrameClock& clock) const {
    return {hover_.value(clock), press_.value(clock), focus_.value(clock), disabled_.value(clock),
            checked_.value(clock)};
}

Color StateLayer::surface(Color base, Color content, const Sample& s, const Theme& th) {
    // Overlays are suppressed as the control becomes disabled.
    float active = 1 - s.disabled;
    float overlay = (s.hover * th.hover_opacity + s.press * th.press_opacity) * active;
    Color c = overlay > 0 ? Color::over(content.with_opacity(overlay), base) : base;
    return c.faded(1 - s.disabled * (1 - th.disabled_opacity));
}

Color StateLayer::content(Color c, const Sample& s, const Theme& th) {
    return c.faded(1 - s.disabled * (1 - th.disabled_opacity));
}

void StateLayer::paint_focus_ring(Canvas& canvas, const Rect& r, float radius, const Sample& s,
                                  const Theme& th) {
    if (s.focus_ring <= 0) return;
    float off = th.focus_ring_offset + th.focus_ring_width / 2;
    Rect ring = r.inset(-off, -off);
    canvas.draw_rrect(ring, radius + off, Paint::stroke(th.focus_ring.faded(s.focus_ring), th.focus_ring_width));
}

} // namespace uitk
