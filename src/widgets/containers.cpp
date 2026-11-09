#include "uitk/widgets/containers.h"

#include <algorithm>
#include <cmath>

#include "uitk/widgets/label.h"
#include "uitk/widgets/widget_host.h"

namespace uitk {

// ---------------------------------------------------------------------------- ScrollBar

namespace {
constexpr auto kBarLinger = std::chrono::milliseconds(900);   // stays visible after scrolling
}

ScrollBar::ScrollBar(Orientation o) : orientation_(o) {
    if (vertical()) set_align(Align::End, Align::Fill); else set_align(Align::Fill, Align::End);
}

void ScrollBar::set_metrics(float content, float viewport, float offset) {
    if (content == content_ && viewport == viewport_ && offset == offset_) return;
    content_ = content; viewport_ = viewport; offset_ = offset;
    invalidate();
}

void ScrollBar::flash() { last_activity_ = AnimClock::now(); invalidate(); }

Size ScrollBar::measure(float, const Theme& t) {
    return vertical() ? Size{t.scrollbar_width, 0} : Size{0, t.scrollbar_width};
}

Rect ScrollBar::thumb() const {
    if (!needed()) return {};
    float len = vertical() ? size().height : size().width;
    float th = std::max(24.f, len * viewport_ / content_);
    float range = std::max(1.f, content_ - viewport_);
    float at = (len - th) * std::clamp(offset_ / range, 0.f, 1.f);
    return vertical() ? Rect{2, at, size().width - 4, th} : Rect{at, 2, th, size().height - 4};
}

void ScrollBar::paint(PaintContext& ctx) {
    if (!needed()) return;
    const Theme& t = ctx.theme;
    // Visible while the pointer is over the scrolled area / the bar, while dragging, and for a
    // moment after any scroll; fades in and out.
    bool active = (parent() && (parent()->state() & STATE_HOVER_WITHIN)) || dragging_ ||
                  AnimClock::now() - last_activity_ < kBarLinger;
    visible_.set(active ? 1.f : 0.f, {Millis(active ? 100 : 400), Easing::ease_out_quad()});
    float v = visible_.value(ctx.clock);
    if (!active && v > 0) ctx.clock.request_frame();
    if (active && !dragging_ && !(parent()->state() & STATE_HOVER_WITHIN)) ctx.clock.request_frame();   // linger timer
    if (v <= 0) return;
    StateLayer::Sample s = layer_.sample(ctx.clock);
    float strength = 1 + 0.5f * s.hover + 0.8f * s.press;
    Color c = t.scrollbar_thumb.faded(std::min(1.f, v * strength));
    c.a = (uint8_t)std::min(255.f, c.a * strength);
    Rect th = thumb();
    ctx.canvas.draw_rrect(th, std::min(th.width, th.height) / 2, Paint::fill(c));
}

bool ScrollBar::on_pointer(const PointerEvent& e) {
    if (!needed()) return false;
    Rect th = thumb();
    float range = std::max(1.f, content_ - viewport_);
    bool v = vertical();
    float along = v ? e.pos.y : e.pos.x, start = v ? th.y : th.x, len = v ? th.height : th.width;
    float track = std::max(1.f, (v ? size().height : size().width) - len);
    switch (e.type) {
        case PointerEvent::Type::Down:
            if (e.button != PointerButton::Left) return true;
            if (along >= start && along < start + len) { dragging_ = true; grab_ = along - start; }
            else scrolled.emit(offset_ + (along < start ? -viewport_ : viewport_) * 0.9f);   // page
            on_state_changed(state());
            return true;
        case PointerEvent::Type::Move:
            if (dragging_) scrolled.emit((along - grab_) / track * range);
            return dragging_;
        case PointerEvent::Type::Up:
            dragging_ = false;
            flash();
            on_state_changed(state());
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------- ScrollView

ScrollView::ScrollView() {
    set_clip_children(true);
    set_expand(true, true);
    bar_ = &add<ScrollBar>();   // painted above the content
    bar_->scrolled.connect([this](float y) { scroll_to(y, false); });
}

Size ScrollView::measure(float available_width, const Theme& t) {
    if (!content_) return {};
    return content_->measure_clamped(available_width, t);
}

void ScrollView::layout(const Theme& t) {
    float w = size().width, h = size().height;
    content_height_ = content_ ? content_->measure_clamped(w, t).height : 0;
    target_ = std::clamp(target_, 0.f, max_offset());
    if (content_) {
        content_->set_bounds({0, -std::round(shown_), w, std::max(content_height_, h)});
        content_->layout(t);
    }
    bar_->set_bounds({w - t.scrollbar_width, 0, t.scrollbar_width, h});
    bar_->set_metrics(content_height_, h, shown_);
}

void ScrollView::apply_offset(float y) {
    shown_ = y;
    if (content_) {
        Rect b = content_->bounds();
        content_->set_bounds({b.x, -std::round(y), b.width, b.height});
    }
    bar_->set_metrics(content_height_, size().height, y);
}

void ScrollView::paint(PaintContext& ctx) {
    // Follow the spring; children are painted after this, at the updated position.
    float y = spring_.value(ctx.clock);
    if (y != shown_) apply_offset(y);
}

void ScrollView::scroll_to(float y, bool animate) {
    y = std::clamp(y, 0.f, max_offset());
    if (y == target_ && (animate || y == shown_)) return;
    target_ = y;
    if (animate) spring_.set_target(y);
    else { spring_.jump(y); apply_offset(y); }
    bar_->flash();
    invalidate();
}

bool ScrollView::on_scroll(const ScrollEvent& e) {
    // Shift+wheel is horizontal scrolling: leave it to horizontal containers (tables).
    if (max_offset() <= 0 || e.dy == 0 || (e.mods & MOD_SHIFT)) return false;
    scroll_to(target_ + e.dy * 3);   // one wheel notch ≈ 45 px
    return true;
}

bool ScrollView::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    float page = size().height * 0.9f;
    switch (e.keysym) {
        case key::PageDown: scroll_to(target_ + page); return true;
        case key::PageUp:   scroll_to(target_ - page); return true;
    }
    return false;
}

void ScrollView::reveal_area(Widget* w, const Rect& area) {
    if (!content_ || !content_->is_ancestor_of(w)) return;
    // The area in content coordinates (independent of the current offset).
    Point top = w->to_window(area.origin()), origin = content_->to_window({0, 0});
    float y0 = top.y - origin.y, y1 = y0 + area.height;
    const float margin = 12;
    if (y0 - margin < target_) scroll_to(y0 - margin);
    else if (y1 + margin > target_ + size().height) scroll_to(y1 + margin - size().height);
}

// ---------------------------------------------------------------------------- TabView

TabView::TabView() {
    strip_ = &add<Box>(Orientation::Horizontal, 0.f);
    pages_ = &add<Widget>();
}

void TabView::add_tab_button(std::string title, Widget* page) {
    int index = (int)tabs_.size();
    ToggleButton& tab = strip_->add<ToggleButton>(std::move(title), ButtonStyle::Tab);
    tab.clicked.connect([this, index] { set_current(index); });
    tabs_.push_back(&tab);
    page_list_.push_back(page);
    page->set_visible(false);
    if (current_ < 0) set_current(0);
}

void TabView::set_current(int index) {
    if (index < 0 || index >= (int)tabs_.size()) return;
    bool changed_now = index != current_;
    current_ = index;
    for (int i = 0; i < (int)tabs_.size(); ++i) {
        tabs_[(size_t)i]->set_checked(i == index);
        page_list_[(size_t)i]->set_visible(i == index);
    }
    invalidate_layout();
    if (changed_now) changed.emit(index);
}

Size TabView::measure(float available_width, const Theme& t) {
    Size s = strip_->measure_clamped(available_width, t);
    float h = 0, w = s.width;
    if (current_ >= 0) {
        Size p = page_list_[(size_t)current_]->measure_clamped(available_width, t);
        h = p.height;
        w = std::max(w, p.width);
    }
    return {w, s.height + 1 + h};
}

void TabView::layout(const Theme& t) {
    float w = size().width;
    float sh = strip_->measure_clamped(w, t).height;
    strip_->set_bounds({0, 0, w, sh});
    strip_->layout(t);
    pages_->set_bounds({0, sh + 1, w, std::max(0.f, size().height - sh - 1)});
    for (Widget* p : page_list_) {
        if (!p->visible()) continue;
        p->set_bounds(Rect::from_size(pages_->size()));
        p->layout(t);
    }
    if (current_ >= 0) {
        // The indicator slides under the active tab.
        const Rect& b = tabs_[(size_t)current_]->bounds();
        Rect target{b.x, sh - 2, b.width, 2};
        if (indicator_.target().width == 0) indicator_.jump(target);
        else indicator_.set(target, {Millis(180), Easing::ease_out_cubic()});
    }
}

void TabView::paint_over(PaintContext& ctx) {
    float sh = strip_->size().height;
    ctx.canvas.draw_rect({0, sh, size().width, 1}, Paint::fill(ctx.theme.border));
    Rect ind = indicator_.value(ctx.clock);
    if (ind.width > 0) ctx.canvas.draw_rect(ind, Paint::fill(ctx.theme.accent));
}

bool TabView::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    WidgetHost* h = host();
    bool in_strip = h && h->focus() && strip_->is_ancestor_of(h->focus());
    if (!in_strip) return false;
    int n = (int)tabs_.size();
    if (e.keysym == key::Left || e.keysym == key::Right) {
        int i = (current_ + (e.keysym == key::Right ? 1 : n - 1)) % n;
        set_current(i);
        h->set_focus(tabs_[(size_t)i], true);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------- Badge

Badge::Badge(std::string text) : text_(std::move(text)) { set_align(Align::Start, Align::Center); }

Size Badge::measure(float, const Theme& t) {
    TextStyle s = Label::style_for(t, TextRole::Caption, TextAlign::Left, true);
    s.weight = 600;
    float w = TextLayout(text_, s).size().width;
    return {std::max(t.badge_height, std::ceil(w) + 12), t.badge_height};
}

void Badge::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    Rect r = local_rect();
    ctx.canvas.draw_rrect(r, r.height / 2, Paint::fill(t.badge_bg));
    TextStyle s = Label::style_for(t, TextRole::Caption, TextAlign::Center, true);
    s.weight = 600;
    s.color = t.badge_fg;
    TextLayout tl(text_, s, r.width);
    ctx.canvas.draw_text(tl, {0, std::round((r.height - tl.size().height) / 2)});
}

} // namespace uitk
