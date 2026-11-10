#include "uitk/widgets/color_swatch.h"

#include <algorithm>

#include "uitk/widgets/glyphs.h"

namespace uitk {

namespace {
Color on_color(Color c) {
    const Color white = Color::hex(0xffffff), black = Color::hex(0x000000);
    return contrast_ratio(c, white) >= contrast_ratio(c, black) ? white : black;
}
} // namespace

ColorSwatch::ColorSwatch(Color color) : Button(std::string{}, ButtonStyle::Circular), color_(color) {}

void ColorSwatch::set_checked(bool c) {
    if (c == checked()) return;
    set_checked_state(c);
    set_icon(c ? glyph(Glyph::Check) : Image{});
}

ButtonLook ColorSwatch::look(const Theme& t) const {
    ButtonLook l = Button::look(t);
    Color on = on_color(color_);
    l.colors = {color_, on, on};
    l.circle = true;
    l.height = t.control_height;
    return l;
}

void ColorSwatch::paint_over(PaintContext& ctx) {
    Button::paint_over(ctx);
    Rect r = local_rect();
    float radius = std::min(r.width, r.height) / 2;
    // A hairline in the text colour keeps swatches that match the window visible.
    ctx.canvas.draw_circle(r.center(), radius - 0.5f, Paint::stroke(ctx.theme.foreground.with_opacity(0.15f), 1));
    if (checked()) {   // window-coloured gap, then a ring in the text colour
        ctx.canvas.draw_circle(r.center(), radius - 2.5f, Paint::stroke(ctx.theme.window_bg, 2));
        ctx.canvas.draw_circle(r.center(), radius - 1, Paint::stroke(ctx.theme.foreground, 2));
    }
}

ColorSwatchPicker::ColorSwatchPicker(std::vector<Color> colors, int selected) : Box(Orientation::Horizontal, 8.f) {
    set_colors(std::move(colors));
    set_selected(selected);
}

void ColorSwatchPicker::set_colors(std::vector<Color> colors) {
    if (colors.size() == swatches_.size()) {   // same count: recolour in place (keeps focus)
        for (size_t i = 0; i < colors.size(); ++i) swatches_[i]->set_color(colors[i]);
        return;
    }
    clear_children();
    swatches_.clear();
    for (size_t i = 0; i < colors.size(); ++i) {
        ColorSwatch& s = add<ColorSwatch>(colors[i]);
        s.clicked.connect([this, i] {
            set_selected((int)i);
            changed.emit((int)i);
        });
        swatches_.push_back(&s);
    }
    if (selected_ >= (int)swatches_.size()) selected_ = -1;
    set_selected(selected_);
}

void ColorSwatchPicker::set_selected(int index) {
    selected_ = index;
    for (size_t i = 0; i < swatches_.size(); ++i) swatches_[i]->set_checked((int)i == index);
}

} // namespace uitk
