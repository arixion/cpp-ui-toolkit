#include "uitk/widgets/window_frame.h"

#include <algorithm>

namespace uitk {

WindowFrame::WindowFrame(Window* window) : window_(window) { set_shadow(true); }

void WindowFrame::set_shadow(bool on) {
    shadow_ = on;
    if (window_) window_->set_shadow_insets(on ? shadow_insets() : Insets{}, on ? RESIZE_GRIP : 0);
}

unsigned WindowFrame::resize_edges(Point p, Size s, const Theme& theme) const {
    if (maximized_) return EDGE_NONE;
    float corner = std::max(BORDER, theme.window_radius);
    bool top = p.y < BORDER, bottom = p.y >= s.height - BORDER;
    bool left = p.x < BORDER, right = p.x >= s.width - BORDER;
    if ((top || bottom) && p.x < corner) left = true;
    if ((top || bottom) && p.x >= s.width - corner) right = true;
    if ((left || right) && p.y < corner) top = true;
    if ((left || right) && p.y >= s.height - corner) bottom = true;
    return (top ? EDGE_TOP : 0) | (bottom ? EDGE_BOTTOM : 0) | (left ? EDGE_LEFT : 0) | (right ? EDGE_RIGHT : 0);
}

MouseCursor WindowFrame::cursor_for_edges(unsigned e) {
    switch (e) {
        case EDGE_TOP:                return MouseCursor::ResizeN;
        case EDGE_BOTTOM:             return MouseCursor::ResizeS;
        case EDGE_LEFT:               return MouseCursor::ResizeW;
        case EDGE_RIGHT:              return MouseCursor::ResizeE;
        case EDGE_TOP | EDGE_LEFT:    return MouseCursor::ResizeNW;
        case EDGE_TOP | EDGE_RIGHT:   return MouseCursor::ResizeNE;
        case EDGE_BOTTOM | EDGE_LEFT: return MouseCursor::ResizeSW;
        case EDGE_BOTTOM | EDGE_RIGHT:return MouseCursor::ResizeSE;
    }
    return MouseCursor::Default;
}

void WindowFrame::begin(Canvas& c, Size s, const Theme& th, float active) {
    Rect frame = Rect::from_size(s);
    if (shadow_ && !maximized_) {
        // Layered like GNOME/libadwaita: a soft ambient shadow plus a tight contact shadow,
        // cross-faded between the focused and backdrop variants (cached; opacity only).
        // Hollow: the opaque window covers the interior, so it is never filled.
        using M = Canvas::ShadowMode;
        Color sh = th.shadow;
        float r = th.window_radius;
        c.draw_shadow(frame, r, sh.with_alpha(56), 7.f, {0, 3}, 0, M::Hollow, active);
        c.draw_shadow(frame, r, sh.with_alpha(38), 1.5f, {0, 1}, 0, M::Hollow, active);
        c.draw_shadow(frame, r, sh.with_alpha(30), 5.f, {0, 2}, 0, M::Hollow, 1 - active);
        c.draw_shadow(frame, r, sh.with_alpha(26), 1.f, {0, 1}, 0, M::Hollow, 1 - active);
    }
    c.save();
    if (!maximized_) c.clip_rrect(frame, th.window_radius);   // anti-aliased rounded frame
    c.draw_rect(frame, Paint::fill(th.window_bg));
}

void WindowFrame::end(Canvas& c) { c.restore(); }

} // namespace uitk
