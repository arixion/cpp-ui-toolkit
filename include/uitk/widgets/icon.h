// Icon: a symbolic image tinted with a theme colour (follows theme changes, selection, disabled).
#pragma once
#include <optional>

#include "uitk/widgets/widget.h"

namespace uitk {

class Icon : public Widget {
public:
    explicit Icon(Image image = {}, float size = 0 /* theme icon size */);
    void set_image(Image image) { image_ = std::move(image); invalidate(); }
    void set_size(float px) { size_ = px; invalidate_layout(); }
    void set_token(ColorToken t) { if (t != token_) { token_ = t; invalidate(); } }   // theme colour (default Foreground)
    void set_color(std::optional<Color> c) { if (c != color_) { color_ = c; invalidate(); } }   // fixed colour

    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;

private:
    Image image_;
    float size_;
    ColorToken token_ = ColorToken::Foreground;
    std::optional<Color> color_;
};

} // namespace uitk
