// Paned: two panes with a draggable divider (splitter / sash), side by side or stacked.
//
//   auto& split = box.add<Paned>(Orientation::Horizontal);
//   auto& side = split.set_start<TreeView>();
//   auto& editor = split.set_end<Box>();
//   split.set_position(260);          // size of the start pane
//   split.set_min_sizes(160, 300);
//
// The divider is a 1 px line with a wider invisible grab area; it highlights in the accent
// colour while hovered or dragged. Double-click restores the initial position. When focused
// (Tab), arrow keys move it (Shift: 50 px steps). Nest Paneds for three or more panes.
// Hiding a pane (set_visible(false)) gives the other one the whole area (toggle a side bar).
#pragma once
#include <memory>

#include "uitk/core/signal.h"
#include "uitk/widgets/widget.h"

namespace uitk {

// Which pane keeps its size when the Paned is resized.
enum class PanedResize { KeepStart, KeepEnd, Proportional };

class Paned : public Widget {
public:
    explicit Paned(Orientation orientation = Orientation::Horizontal);

    template <class W, class... Args>
    W& set_start(Args&&... args) { return static_cast<W&>(set_pane(0, std::make_unique<W>(std::forward<Args>(args)...))); }
    template <class W, class... Args>
    W& set_end(Args&&... args) { return static_cast<W&>(set_pane(1, std::make_unique<W>(std::forward<Args>(args)...))); }
    Widget* start() const { return panes_[0]; }
    Widget* end() const { return panes_[1]; }

    void set_position(float start_size);   // px; clamped to the minimum sizes at layout
    float position() const { return position_; }
    void set_min_sizes(float start, float end) { min_[0] = start; min_[1] = end; invalidate_layout(); }
    void set_resize_policy(PanedResize p) { policy_ = p; }
    Orientation orientation() const { return orientation_; }

    Signal<float> position_changed;   // while dragging / keyboard

    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    void paint_over(PaintContext& ctx) override;

    static constexpr float kGrab = 7;   // grab area thickness

private:
    class Sash;
    friend class Sash;
    Widget& set_pane(int i, std::unique_ptr<Widget> w);
    float total() const { return orientation_ == Orientation::Horizontal ? size().width : size().height; }
    float clamp_position(float p) const;
    void drag_to(float p);

    Orientation orientation_;
    Widget* panes_[2] = {nullptr, nullptr};
    Sash* sash_;
    float position_ = -1;        // < 0: the start pane's natural size
    float initial_ = -1;
    float min_[2] = {80, 80};
    float last_total_ = -1;
    PanedResize policy_ = PanedResize::KeepStart;
};

} // namespace uitk
