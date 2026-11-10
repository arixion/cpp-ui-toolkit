// ColorSwatch: a round, checkable colour chip. ColorSwatchPicker: a row of them with one
// selected (accent pickers, label colours, palette choices).
//
//   auto& p = box.add<ColorSwatchPicker>(std::vector<Color>{blue, green, red}, 0);
//   p.changed.connect([](int i) { ... });
#pragma once
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/button.h"

namespace uitk {

class ColorSwatch : public Button {
public:
    explicit ColorSwatch(Color color);

    void set_color(Color c) { if (c != color_) { color_ = c; invalidate(); } }
    Color color() const { return color_; }
    void set_checked(bool checked);   // shows the selection ring and check mark
    bool checked() const { return state() & STATE_CHECKED; }

    void paint_over(PaintContext& ctx) override;

protected:
    ButtonLook look(const Theme& t) const override;

private:
    Color color_;
};

class ColorSwatchPicker : public Box {
public:
    explicit ColorSwatchPicker(std::vector<Color> colors = {}, int selected = -1);

    Signal<int> changed;   // user picked a swatch (not emitted by set_selected)
    void set_colors(std::vector<Color> colors);
    void set_selected(int index);   // -1 = none
    int selected() const { return selected_; }
    ColorSwatch& swatch(int index) { return *swatches_[(size_t)index]; }
    int count() const { return (int)swatches_.size(); }

private:
    std::vector<ColorSwatch*> swatches_;
    int selected_ = -1;
};

} // namespace uitk
