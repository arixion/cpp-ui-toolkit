#include "uitk/widgets/frame.h"

namespace uitk {

Frame::Frame(FrameStyle style, Orientation o, float spacing) : Box(o, spacing), style_(style) {}

void Frame::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    Rect r = local_rect();
    // Hairline borders sit on half-pixels so they render as one crisp device pixel at 1x.
    Rect hair = r.inset(0.5f, 0.5f);
    switch (style_) {
        case FrameStyle::Card:
            ctx.canvas.draw_rrect(r, t.card_radius, Paint::fill(t.surface_bg));
            ctx.canvas.draw_rrect(hair, t.card_radius - 0.5f, Paint::stroke(t.border, 1));
            break;
        case FrameStyle::Input:
            ctx.canvas.draw_rrect(r, t.radius, Paint::fill(t.input_bg));
            ctx.canvas.draw_rrect(hair, t.radius - 0.5f, Paint::stroke(t.input_border, 1));
            break;
        case FrameStyle::Panel:
            ctx.canvas.draw_rect(hair, Paint::stroke(t.border, 1));
            break;
    }
}

Separator::Separator(Orientation o) : orientation_(o) {
    if (o == Orientation::Horizontal) set_align(Align::Fill, Align::Start);
    else set_align(Align::Start, Align::Fill);
}

Size Separator::measure(float, const Theme&) {
    return orientation_ == Orientation::Horizontal ? Size{1, 1} : Size{1, 1};
}

void Separator::paint(PaintContext& ctx) {
    ctx.canvas.draw_rect(local_rect(), Paint::fill(ctx.theme.border));
}

} // namespace uitk
