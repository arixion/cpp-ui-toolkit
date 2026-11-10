// Frame: a Box that draws a themed surface behind its children (cards, notifications, inputs,
// panels). Separator: a 1 px themed divider line.
#pragma once
#include "uitk/widgets/box.h"

namespace uitk {

enum class FrameStyle {
    Card,    // surface_bg + border, card radius (notifications, dialogs, popovers)
    Input,   // input_bg + input border, control radius (search fields, command center)
    Panel,   // border only, no rounding
};

class Frame : public Box {
public:
    explicit Frame(FrameStyle style = FrameStyle::Card, Orientation o = Orientation::Vertical, float spacing = -1);
    void set_frame_style(FrameStyle s) { style_ = s; invalidate(); }
    void paint(PaintContext& ctx) override;

private:
    FrameStyle style_;
};

class Separator : public Widget {
public:
    explicit Separator(Orientation o = Orientation::Horizontal);
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;

private:
    Orientation orientation_;
};

} // namespace uitk
