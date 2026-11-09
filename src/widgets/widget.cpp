#include "uitk/widgets/widget.h"

#include <algorithm>

#include "uitk/widgets/widget_host.h"

namespace uitk {

Widget& Widget::add_child(std::unique_ptr<Widget> child) {
    return insert_child(children_.size(), std::move(child));
}

Widget& Widget::insert_child(size_t index, std::unique_ptr<Widget> child) {
    child->parent_ = this;
    index = std::min(index, children_.size());
    Widget& w = **children_.insert(children_.begin() + (long)index, std::move(child));
    w.refresh_enabled_state();
    invalidate_layout();
    return w;
}

std::unique_ptr<Widget> Widget::remove_child(Widget* child) {
    auto it = std::find_if(children_.begin(), children_.end(), [&](auto& c) { return c.get() == child; });
    if (it == children_.end()) return nullptr;
    if (WidgetHost* h = host()) h->widget_removed(child);   // drop hover/focus/capture first
    std::unique_ptr<Widget> out = std::move(*it);
    children_.erase(it);
    out->parent_ = nullptr;
    invalidate_layout();
    return out;
}

void Widget::clear_children() {
    while (!children_.empty()) remove_child(children_.back().get());
}

WidgetHost* Widget::host() const {
    const Widget* w = this;
    while (w->parent_) w = w->parent_;
    return w->host_;
}

bool Widget::is_ancestor_of(const Widget* w) const {
    for (; w; w = w->parent_) if (w == this) return true;
    return false;
}

void Widget::set_bounds(const Rect& r) {
    if (r == bounds_) return;
    bool resized = r.size() != bounds_.size();
    bounds_ = r;
    if (resized) invalidate_layout(); else invalidate();
}

Point Widget::to_window(Point p) const {
    for (const Widget* w = this; w; w = w->parent_) p = p + w->bounds_.origin();
    return p;
}

Point Widget::from_window(Point p) const {
    for (const Widget* w = this; w; w = w->parent_) p = p - w->bounds_.origin();
    return p;
}

Size Widget::measure(float, const Theme&) { return min_size_; }

Size Widget::measure_clamped(float available_width, const Theme& theme) {
    if (fixed_size_.width > 0 && fixed_size_.height > 0) return fixed_size_;
    // Cached per available width: nested layouts ask the same question many times per pass.
    if (measure_theme_ == &theme)
        for (auto& e : measure_cache_) if (e.available == available_width) return e.size;
    if (measure_theme_ != &theme) { measure_cache_[0] = measure_cache_[1] = {}; measure_theme_ = &theme; }
    Size result = [&] {
    Size s = measure(fixed_size_.width > 0 ? fixed_size_.width : available_width, theme);
    s = {std::max(s.width, min_size_.width), std::max(s.height, min_size_.height)};
    if (fixed_size_.width > 0) s.width = fixed_size_.width;
    if (fixed_size_.height > 0) s.height = fixed_size_.height;
    return s;
    }();
    measure_cache_[measure_cache_next_] = {available_width, result};
    measure_cache_next_ ^= 1;
    return result;
}

void Widget::drop_measure_cache_recursive() {
    measure_cache_[0] = measure_cache_[1] = {};
    for (auto& c : children_) c->drop_measure_cache_recursive();
}

void Widget::layout(const Theme& theme) {
    for (auto& c : children_) {
        if (!c->visible()) continue;
        c->set_bounds(local_rect());
        c->layout(theme);
    }
}

void Widget::set_visible(bool v) {
    if (v == visible_) return;
    visible_ = v;
    if (!v) if (WidgetHost* h = host()) h->widget_removed(this);   // hidden: no hover/focus
    if (parent_) parent_->invalidate_layout(); else invalidate_layout();
}

void Widget::set_enabled(bool e) {
    if (e == enabled_) return;
    enabled_ = e;
    if (!e) if (WidgetHost* h = host()) h->widget_removed(this);   // disabled: drop focus/press
    refresh_enabled_state();
}

bool Widget::enabled() const {
    for (const Widget* w = this; w; w = w->parent_) if (!w->enabled_) return false;
    return true;
}

void Widget::refresh_enabled_state() {
    set_state_flag(STATE_DISABLED, !enabled());
    for (auto& c : children_) c->refresh_enabled_state();
}

void Widget::scroll_into_view(const Rect& area) {
    for (Widget* a = parent_; a; a = a->parent_) a->reveal_area(this, area);
}

Rect Widget::visible_rect() const {
    Point o = to_window({0, 0});
    Rect vis{o.x, o.y, bounds_.width, bounds_.height};   // window coordinates
    for (const Widget* a = parent_; a; a = a->parent_) {
        if (!a->clip_children_) continue;
        Point ao = a->to_window({0, 0});
        vis = vis.intersect({ao.x, ao.y, a->bounds_.width, a->bounds_.height});
    }
    return {vis.x - o.x, vis.y - o.y, vis.width, vis.height};
}

void Widget::grab_focus() {
    if (WidgetHost* h = host()) h->set_focus(this, false);
}

void Widget::invalidate() {
    if (WidgetHost* h = host()) h->request_redraw();
}

void Widget::invalidate_layout() {
    for (Widget* w = this; w; w = w->parent_) w->measure_cache_[0] = w->measure_cache_[1] = {};
    if (WidgetHost* h = host()) h->request_layout();
}

const Theme& Widget::theme() const {
    static const Theme fallback = Theme::light();
    WidgetHost* h = host();
    return h ? h->theme() : fallback;
}

void Widget::set_state_flag(uint32_t flag, bool on) {
    uint32_t old = state_;
    state_ = on ? (state_ | flag) : (state_ & ~flag);
    if (state_ != old) {
        on_state_changed(old);
        invalidate();
    }
}

} // namespace uitk
