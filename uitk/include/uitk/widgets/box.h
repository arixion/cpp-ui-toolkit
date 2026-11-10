// Box: lays children out in a row or column (the basic building block of every UI).
//   - spacing between children (theme spacing by default), padding around them
//   - children with expand (Widget::set_expand) share the leftover main-axis space
//   - cross-axis placement from each child's Widget::halign()/valign()
#pragma once
#include <vector>

#include "uitk/widgets/widget.h"

namespace uitk {

enum class Justify { Start, Center, End, SpaceBetween };

class Box : public Widget {
public:
    explicit Box(Orientation o = Orientation::Vertical, float spacing = -1 /* theme spacing */);

    void set_orientation(Orientation o) { orientation_ = o; invalidate_layout(); }
    void set_spacing(float s) { spacing_ = s; invalidate_layout(); }
    void set_padding(Insets p) { padding_ = p; invalidate_layout(); }
    void set_padding(float all) { set_padding(Insets{all, all, all, all}); }
    void set_justify(Justify j) { justify_ = j; invalidate_layout(); }
    Orientation orientation() const { return orientation_; }

    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;

protected:
    struct Allocation {
        struct Item { Widget* w; Size natural; float length; };   // length = allocated main size
        std::vector<Item> items;
        float gap = 0, extra = 0;
        int expanders = 0;
    };
    Allocation allocate(float inner_main, float inner_cross, const Theme& theme);

private:
    float spacing(const Theme& t) const { return spacing_ >= 0 ? spacing_ : t.spacing; }

    Orientation orientation_;
    float spacing_;
    Insets padding_;
    Justify justify_ = Justify::Start;
};

} // namespace uitk
