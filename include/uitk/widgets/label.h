// Label: themed text. Styling comes from a TextRole (size/weight/colour from the theme), so text
// stays consistent across controls and follows theme changes.
#pragma once
#include <optional>
#include <string>

#include "uitk/widgets/widget.h"

namespace uitk {

class Label : public Widget {
public:
    explicit Label(std::string text = {}, TextRole role = TextRole::Body);

    void set_text(std::string text);
    const std::string& text() const { return text_; }
    void set_role(TextRole role) { role_ = role; invalidate_layout(); }
    void set_wrap(bool wrap) { wrap_ = wrap; invalidate_layout(); }              // multi-line
    void set_ellipsize(bool e) { ellipsize_ = e; invalidate_layout(); }          // single line + "…"
    void set_text_align(TextAlign a) { align_ = a; invalidate(); }
    void set_color(std::optional<Color> c) { if (c != color_) { color_ = c; invalidate(); } }   // fixed colour
    // Theme colour by name (resolved at paint: follows theme changes). Overrides the role colour.
    void set_color_token(std::optional<ColorToken> t) { if (t != token_) { token_ = t; invalidate(); } }
    void set_backdrop_dim(bool d) { backdrop_dim_ = d; invalidate(); }           // dim in inactive windows

    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;

    // Shared helper: the TextStyle a label would use (other widgets reuse it for their text).
    static TextStyle style_for(const Theme& theme, TextRole role, TextAlign align, bool single_line);

private:
    std::string text_;
    TextRole role_;
    TextAlign align_ = TextAlign::Left;
    bool wrap_ = false, ellipsize_ = false, backdrop_dim_ = false;
    std::optional<Color> color_;
    std::optional<ColorToken> token_;
};

} // namespace uitk
