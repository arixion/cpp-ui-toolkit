#include "uitk/widgets/icon.h"

#include <cmath>

namespace uitk {

Icon::Icon(Image image, float size) : image_(std::move(image)), size_(size) {
    set_align(Align::Start, Align::Center);
}

Size Icon::measure(float, const Theme& t) {
    float s = size_ > 0 ? size_ : t.icon_size;
    return {s, s};
}

void Icon::paint(PaintContext& ctx) {
    if (!image_.valid()) return;
    float s = size_ > 0 ? size_ : ctx.theme.icon_size;
    Color c = color_ ? *color_ : ctx.theme.color(token_);
    if (state() & STATE_DISABLED) c = c.faded(ctx.theme.disabled_opacity);
    Rect r = local_rect();
    ctx.canvas.draw_image(image_, {std::round((r.width - s) / 2), std::round((r.height - s) / 2), s, s}, c);
}

} // namespace uitk
