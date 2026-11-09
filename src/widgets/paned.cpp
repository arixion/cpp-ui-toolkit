#include "uitk/widgets/paned.h"

#include <algorithm>
#include <cmath>

#include "uitk/widgets/state.h"

namespace uitk {

// The grab area over the divider: cursor, hover/drag highlight, dragging and keyboard.
class Paned::Sash : public Widget {
public:
    explicit Sash(Paned& owner) : owner_(owner) {}
    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    MouseCursor cursor() const override {
        return owner_.orientation_ == Orientation::Horizontal ? MouseCursor::ResizeE : MouseCursor::ResizeS;
    }
    bool dragging() const { return dragging_; }
    float highlight(FrameClock& clock) const {
        StateLayer::Sample s = layer_.sample(clock);
        return std::max({s.hover, s.press, s.focus_ring});
    }

    bool on_pointer(const PointerEvent& e) override {
        bool h = owner_.orientation_ == Orientation::Horizontal;
        float along = h ? e.pos.x : e.pos.y;
        switch (e.type) {
            case PointerEvent::Type::Down:
                if (e.button != PointerButton::Left) return true;
                if (e.click_count >= 2 && owner_.initial_ >= 0) { owner_.drag_to(owner_.initial_); return true; }
                dragging_ = true;
                grab_ = along;
                return true;
            case PointerEvent::Type::Move:
                if (dragging_) owner_.drag_to(owner_.position_ + (along - grab_));
                return dragging_;
            case PointerEvent::Type::Up:
                dragging_ = false;
                on_state_changed(state());
                return true;
            default: return false;
        }
    }
    bool on_key(const KeyEvent& e) override {
        if (!e.pressed) return false;
        float step = (e.mods & MOD_SHIFT) ? 50.f : 10.f;
        bool h = owner_.orientation_ == Orientation::Horizontal;
        uint32_t less = h ? key::Left : key::Up, more = h ? key::Right : key::Down;
        if (e.keysym == less) { owner_.drag_to(owner_.position_ - step); return true; }
        if (e.keysym == more) { owner_.drag_to(owner_.position_ + step); return true; }
        if (e.keysym == key::Home) { owner_.drag_to(0); return true; }
        if (e.keysym == key::End) { owner_.drag_to(1e9f); return true; }
        return false;
    }

protected:
    void on_state_changed(uint32_t) override { layer_.update(state() | (dragging_ ? STATE_PRESSED : 0u), theme()); }

private:
    Paned& owner_;
    StateLayer layer_;
    bool dragging_ = false;
    float grab_ = 0;
};

Paned::Paned(Orientation o) : orientation_(o) {
    set_expand(true, true);
    sash_ = &add<Sash>(*this);
}

Widget& Paned::set_pane(int i, std::unique_ptr<Widget> w) {
    if (panes_[i]) remove_child(panes_[i]);
    // Panes are painted (and hit-tested) below the sash.
    Widget& ref = insert_child(i == 0 ? 0 : (panes_[0] ? 1 : 0), std::move(w));
    panes_[i] = &ref;
    invalidate_layout();
    return ref;
}

void Paned::set_position(float p) {
    if (initial_ < 0) initial_ = p;
    if (p == position_) return;
    position_ = p;
    invalidate_layout();
}

float Paned::clamp_position(float p) const {
    float t = total();
    if (t <= 0) return std::max(0.f, p);
    float hi = std::max(min_[0], t - 1 - min_[1]);
    return std::round(std::clamp(p, std::min(min_[0], hi), hi));
}

void Paned::drag_to(float p) {
    p = clamp_position(p);
    if (p == position_) return;
    position_ = p;
    invalidate_layout();
    position_changed.emit(p);
}

Size Paned::measure(float available_width, const Theme& t) {
    bool h = orientation_ == Orientation::Horizontal;
    Size a = panes_[0] ? panes_[0]->measure_clamped(h ? 1e9f : available_width, t) : Size{};
    Size b = panes_[1] ? panes_[1]->measure_clamped(h ? 1e9f : available_width, t) : Size{};
    if (h) return {std::max(a.width, min_[0]) + 1 + std::max(b.width, min_[1]), std::max(a.height, b.height)};
    return {std::max(a.width, b.width), std::max(a.height, min_[0]) + 1 + std::max(b.height, min_[1])};
}

void Paned::layout(const Theme& t) {
    bool h = orientation_ == Orientation::Horizontal;
    float tot = total();
    if (position_ < 0 && panes_[0]) {
        Size s = panes_[0]->measure_clamped(h ? 1e9f : size().width, t);
        position_ = h ? s.width : s.height;
        if (initial_ < 0) initial_ = position_;
    }
    // Window resizes: keep the chosen pane's size.
    if (last_total_ > 0 && tot != last_total_) {
        if (policy_ == PanedResize::KeepEnd) position_ += tot - last_total_;
        else if (policy_ == PanedResize::Proportional) position_ = position_ * tot / last_total_;
    }
    last_total_ = tot;
    position_ = clamp_position(position_);
    float p = position_;
    Size sz = size();
    // A hidden pane (e.g. a toggled-off side bar): the other one takes everything.
    bool show[2] = {panes_[0] && panes_[0]->visible(), panes_[1] && panes_[1]->visible()};
    sash_->set_visible(show[0] && show[1]);
    if (!(show[0] && show[1])) {
        for (int i = 0; i < 2; ++i) if (show[i]) { panes_[i]->set_bounds(local_rect()); panes_[i]->layout(t); }
        return;
    }
    if (panes_[0]) panes_[0]->set_bounds(h ? Rect{0, 0, p, sz.height} : Rect{0, 0, sz.width, p});
    if (panes_[1]) panes_[1]->set_bounds(h ? Rect{p + 1, 0, std::max(0.f, sz.width - p - 1), sz.height}
                                           : Rect{0, p + 1, sz.width, std::max(0.f, sz.height - p - 1)});
    for (Widget* w : panes_) if (w) w->layout(t);
    float g = std::floor(kGrab / 2);
    sash_->set_bounds(h ? Rect{p - g, 0, kGrab, sz.height} : Rect{0, p - g, sz.width, kGrab});
}

void Paned::paint_over(PaintContext& ctx) {
    if (!sash_->visible()) return;
    const Theme& t = ctx.theme;
    bool h = orientation_ == Orientation::Horizontal;
    float p = position_;
    Rect line = h ? Rect{p, 0, 1, size().height} : Rect{0, p, size().width, 1};
    ctx.canvas.draw_rect(line, Paint::fill(t.border));
    // Accent highlight, 4 px wide, centred on the line (VS Code sash).
    float v = sash_->highlight(ctx.clock);
    if (v > 0) {
        Rect band = h ? Rect{p - 1.5f, 0, 4, size().height} : Rect{0, p - 1.5f, size().width, 4};
        ctx.canvas.draw_rect(band, Paint::fill(t.focus_ring.faded(v)));
    }
}

} // namespace uitk
