#include "uitk/widgets/split_button.h"

#include <cmath>

#include "uitk/widgets/glyphs.h"

namespace uitk {

SplitButton::SplitButton(std::string label, ButtonStyle style)
    : Box(Orientation::Horizontal, 0.f), style_(style) {
    set_align(Align::Start, Align::Center);
    main_ = &add<Button>(std::move(label), style);
    main_->set_expand(true, false);   // a stretched split button grows its main part
    main_->set_align(Align::Fill, Align::Fill);
    menu_ = &add<Button>(glyph(Glyph::ChevronDown), style);
    menu_->set_align(Align::Fill, Align::Fill);
    main_->clicked.connect([this] { clicked.emit(); });
    menu_->clicked.connect([this] { menu_requested.emit(); });
}

void SplitButton::layout(const Theme& t) {
    // One outline: only the outer corners are rounded.
    main_->set_corner_radii(CornerRadii::left(t.radius));
    menu_->set_corner_radii(CornerRadii::right(t.radius));
    menu_->set_fixed_size({t.control_height, 0});
    Box::layout(t);
}

void SplitButton::paint_over(PaintContext& ctx) {
    // Divider in the content colour, inset from the top and bottom (VS Code style).
    ButtonLook l = button_look(style_, ctx.theme, false);
    float x = std::round(menu_->bounds().x) - 0.5f;
    float inset = std::round(size().height * 0.22f);
    ctx.canvas.draw_line({x, inset}, {x, size().height - inset},
                         Paint::stroke(l.colors.foreground.with_opacity(0.45f), 1));
}

} // namespace uitk
