#include "uitk/widgets/status_bar.h"

#include <algorithm>

namespace uitk {

StatusBar::StatusBar() : Box(Orientation::Horizontal, 2.f) {
    set_padding(Insets{6, 0, 6, 0});
    spacer_ = &add<Spacer>();
}

Button& StatusBar::make(size_t index, std::string text, Image icon) {
    Button& b = insert<Button>(index, std::move(text), ButtonStyle::StatusItem);
    if (icon.valid()) { b.set_icon(std::move(icon)); b.set_icon_size(14); }
    b.set_focusable(false);   // like VS Code: pointer-only chrome
    b.set_align(Align::Start, Align::Fill);
    return b;
}

Button& StatusBar::add_start(std::string text, Image icon) {
    size_t index = 0;   // insert before the spacer
    for (; index < children().size() && children()[index].get() != spacer_; ++index) {}
    return make(index, std::move(text), std::move(icon));
}

Button& StatusBar::add_end(std::string text, Image icon) {
    return make(children().size(), std::move(text), std::move(icon));
}

Size StatusBar::measure(float available_width, const Theme& t) {
    Size s = Box::measure(available_width, t);
    return {s.width, t.status_bar_height};
}

void StatusBar::paint(PaintContext& ctx) {
    ctx.canvas.draw_rect(local_rect(), Paint::fill(ctx.theme.window_bg));
}

} // namespace uitk
