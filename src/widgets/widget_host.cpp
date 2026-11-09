#include "uitk/widgets/widget_host.h"

#include "uitk/core/application.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/menu.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace uitk {

WidgetHost::WidgetHost(Window* window) : window_(window) {}

namespace {
// Tooltip bubble: a single-line label on the popup surface.
class TooltipBubble : public Widget {
public:
    explicit TooltipBubble(std::string text) { label_ = &add<Label>(std::move(text), TextRole::Body); }
    Size measure(float w, const Theme& t) override {
        Size s = label_->measure_clamped(w, t);
        return {s.width + 16, s.height + 8};
    }
    void layout(const Theme&) override { label_->set_bounds(local_rect().inset(8, 4)); }
    void paint(PaintContext& ctx) override {
        const Theme& t = ctx.theme;
        ctx.canvas.draw_rrect(local_rect(), t.radius, Paint::fill(t.popup_bg));
        ctx.canvas.draw_rrect(local_rect().inset(0.5f, 0.5f), t.radius - 0.5f, Paint::stroke(t.popup_border, 1));
        label_->set_color(t.tooltip_fg);
    }
private:
    Label* label_;
};
} // namespace

WidgetHost::~WidgetHost() {
    if (tooltip_timer_) if (auto* app = Application::current()) app->cancel_timer(tooltip_timer_);
    hovered_ = captured_ = focus_ = nullptr;
    popups_.clear();
    tooltip_.reset();
    if (root_) root_->host_ = nullptr;
}

Widget& WidgetHost::set_root(std::unique_ptr<Widget> root) {
    if (root_) { widget_removed(root_.get()); root_->host_ = nullptr; }
    root_ = std::move(root);
    root_->parent_ = nullptr;
    root_->host_ = this;
    root_->refresh_enabled_state();
    request_layout();
    return *root_;
}

// ---------------------------------------------------------------------------- theme / frame

void WidgetHost::set_theme(const Theme& t, bool animate) {
    bool metrics_changed = !(t.control_height == theme_.target().control_height &&
                             t.font_size == theme_.target().font_size && t.spacing == theme_.target().spacing);
    if (animate && window_) theme_.set(t, t.theme); else theme_.jump(t);
    if (root_) root_->drop_measure_cache_recursive();   // sizes may depend on theme metrics/fonts
    if (metrics_changed) request_layout(); else request_redraw();
}

FrameClock& WidgetHost::clock() {
    if (external_clock_) return *external_clock_;
    return window_ ? window_->frame_clock() : settled_clock_;
}

void WidgetHost::set_window_active(bool active, bool animate) {
    if (animate && window_) active_.set(active ? 1.f : 0.f, theme_.target().backdrop);
    else active_.jump(active ? 1.f : 0.f);
    request_redraw();
}

void WidgetHost::layout(Size size) {
    if (!root_) return;
    if (!layout_dirty_ && size == laid_out_size_) { layout_popups(); return; }
    in_layout_ = true;
    root_->set_bounds(Rect::from_size(size));
    root_->layout(theme_.target());
    in_layout_ = false;
    layout_dirty_ = false;
    laid_out_size_ = size;
    layout_popups();
    // Widgets may have moved under a stationary pointer.
    if (pointer_inside_ && !captured_) set_hovered(hit_test(pointer_));
}

void WidgetHost::layout_popups() {
    const Theme& t = theme_.target();
    Size win = laid_out_size_;
    const float margin = 8;
    using Placement = PopupOptions::Placement;
    auto place = [&](Widget* w, Rect anchor, bool match_width, float gap, Placement placement = Placement::Below) {
        float max_w = std::max(0.f, win.width - 2 * margin);
        Size s = w->measure_clamped(std::min(max_w, 520.f), t);
        float width = std::min(max_w, std::max(s.width, match_width ? anchor.width : 0.f));
        float height = std::min(s.height, std::max(0.f, win.height - 2 * margin));
        float x, y;
        if (placement == Placement::Right) {
            x = anchor.right() + gap;
            if (x + width > win.width - margin && anchor.x - gap - width >= margin) x = anchor.x - gap - width;   // flip left
            x = std::clamp(x, margin, std::max(margin, win.width - margin - width));
            y = anchor.y;
        } else {
            x = std::clamp(anchor.x, margin, std::max(margin, win.width - margin - width));
            y = anchor.bottom() + gap;
            if (y + height > win.height - margin && anchor.y - gap - height >= margin) y = anchor.y - gap - height;   // flip up
        }
        y = std::clamp(y, margin, std::max(margin, win.height - margin - height));
        w->set_bounds({std::round(x), std::round(y), std::round(width), std::round(height)});
        w->layout(t);
    };
    for (auto& p : popups_) {
        if (p.options.centered) {
            const float m = 24;
            Size s = p.widget->measure_clamped(std::max(0.f, win.width - 2 * m), t);
            float w = std::min(s.width, std::max(0.f, win.width - 2 * m));
            float h = std::min(s.height, std::max(0.f, win.height - 2 * m));
            p.widget->set_bounds({std::round((win.width - w) / 2), std::round((win.height - h) / 2), std::round(w), std::round(h)});
            p.widget->layout(t);
        } else {
            place(p.widget.get(), p.anchor, p.options.match_anchor_width, p.options.gap, p.options.placement);
        }
    }
    if (tooltip_) place(tooltip_.get(), {pointer_.x, pointer_.y, 0, 0}, false, 20);
}

void WidgetHost::prepare_frame() {
    FrameClock& clk = clock();
    frame_theme_ = theme_.value(clk);
    frame_active_ = active_.value(clk);
}

void WidgetHost::paint(Canvas& canvas) {
    if (!root_) return;
    prepare_frame();
    PaintContext ctx{canvas, clock(), frame_theme_, frame_active_};
    paint_widget(root_.get(), ctx);
    // Popup layer: each popup casts a soft shadow onto the window below it.
    auto paint_layer = [&](Widget* w) {
        canvas.draw_shadow(w->bounds(), frame_theme_.popup_radius, frame_theme_.shadow.with_alpha(60), 6.f, {0, 2});
        paint_widget(w, ctx);
    };
    for (auto& p : popups_) {
        if (p.options.modal)   // scrim: dims everything below the dialog
            canvas.draw_rect(Rect::from_size(laid_out_size_), Paint::fill(frame_theme_.shadow.with_opacity(0.45f)));
        paint_layer(p.widget.get());
    }
    if (tooltip_) paint_layer(tooltip_.get());
}

void WidgetHost::paint_widget(Widget* w, PaintContext& ctx) {
    if (!w->visible()) return;
    const Rect& b = w->bounds();
    Canvas& c = ctx.canvas;
    c.save();
    c.translate(b.x, b.y);
    w->paint(ctx);
    if (w->clips_children()) { c.save(); c.clip_rect(w->local_rect()); }
    for (auto& child : w->children()) paint_widget(child.get(), ctx);
    if (w->clips_children()) c.restore();
    w->paint_over(ctx);
    c.restore();
}

// ---------------------------------------------------------------------------- invalidation

void WidgetHost::request_redraw() {
    if (window_) window_->request_redraw();
}

void WidgetHost::request_layout() {
    if (in_layout_) return;   // children resizing during our own layout pass
    layout_dirty_ = true;
    request_redraw();
}

void WidgetHost::widget_removed(Widget* subtree) {
    if (!subtree) return;
    if (tooltip_owner_ && subtree->is_ancestor_of(tooltip_owner_)) hide_tooltip();
    if (captured_ && subtree->is_ancestor_of(captured_)) {
        captured_->set_state_flag(STATE_PRESSED, false);
        captured_ = nullptr;
    }
    if (hovered_ && subtree->is_ancestor_of(hovered_)) {
        for (Widget* a = hovered_; a; a = a->parent()) a->set_state_flag(STATE_HOVER_WITHIN, false);
        hovered_->set_state_flag(STATE_HOVERED, false);
        hovered_ = nullptr;
    }
    if (focus_ && subtree->is_ancestor_of(focus_)) {
        focus_->set_state_flag(STATE_FOCUSED | STATE_FOCUS_VISIBLE, false);
        focus_ = nullptr;
    }
}

// ---------------------------------------------------------------------------- hit testing / hover

Widget* WidgetHost::hit_test_tree(Widget* top, Point p, bool interactive_only) const {
    std::function<Widget*(Widget*, Point)> hit = [&](Widget* w, Point local) -> Widget* {
        if (!w->visible() || !w->local_rect().contains(local)) return nullptr;
        const auto& kids = w->children();
        for (auto it = kids.rbegin(); it != kids.rend(); ++it)   // topmost (last painted) first
            if (Widget* r = hit(it->get(), local - (*it)->bounds().origin())) return r;
        if (!interactive_only) return w;   // deepest widget under the point (wheel events)
        return (w->accepts_pointer() && w->enabled()) ? w : nullptr;
    };
    return top ? hit(top, p - top->bounds().origin()) : nullptr;
}

Widget* WidgetHost::hit_test(Point p, bool interactive_only) const {
    // Popups are above the tree; their area blocks what is underneath, and a modal popup blocks
    // everything below it.
    for (auto it = popups_.rbegin(); it != popups_.rend(); ++it) {
        if (it->widget->bounds().contains(p)) return hit_test_tree(it->widget.get(), p, interactive_only);
        if (it->options.modal) return nullptr;
    }
    return hit_test_tree(root_.get(), p, interactive_only);
}

void WidgetHost::set_hovered(Widget* w) {
    if (w == hovered_) return;
    // HOVER_WITHIN on the hovered widget and all its ancestors (CSS :hover semantics), so a
    // container can react while the pointer is over one of its children.
    for (Widget* a = hovered_; a; a = a->parent())
        if (!a->is_ancestor_of(w)) a->set_state_flag(STATE_HOVER_WITHIN, false);
    if (hovered_) hovered_->set_state_flag(STATE_HOVERED, false);
    hovered_ = w;
    if (hovered_) hovered_->set_state_flag(STATE_HOVERED, true);
    for (Widget* a = hovered_; a; a = a->parent()) a->set_state_flag(STATE_HOVER_WITHIN, true);
    update_pressed();
    schedule_tooltip(hovered_);
}

// ---------------------------------------------------------------------------- tooltips

void WidgetHost::schedule_tooltip(Widget* w) {
    if (w == tooltip_owner_ && tooltip_) return;
    hide_tooltip();
    Application* app = Application::current();
    if (!w || w->tooltip().empty() || !app || captured_) return;
    tooltip_owner_ = w;
    tooltip_timer_ = app->add_timer(std::chrono::milliseconds(TOOLTIP_DELAY_MS), [this] {
        tooltip_timer_ = 0;
        if (tooltip_owner_ && tooltip_owner_ == hovered_) show_tooltip(tooltip_owner_, pointer_);
    });
}

void WidgetHost::show_tooltip(Widget* w, Point pos) {
    if (!w || w->tooltip().empty()) return;
    tooltip_owner_ = w;
    pointer_ = pos;
    tooltip_ = std::make_unique<TooltipBubble>(w->tooltip());
    tooltip_->host_ = this;
    layout_popups();
    request_redraw();
}

void WidgetHost::hide_tooltip() {
    if (tooltip_timer_) { if (auto* app = Application::current()) app->cancel_timer(tooltip_timer_); tooltip_timer_ = 0; }
    tooltip_owner_ = nullptr;
    if (tooltip_) { tooltip_.reset(); request_redraw(); }
}

// ---------------------------------------------------------------------------- popups

Widget& WidgetHost::show_popup(std::unique_ptr<Widget> w, Rect anchor, PopupOptions options) {
    hide_tooltip();
    Popup p;
    p.widget = std::move(w);
    p.anchor = anchor;
    p.options = std::move(options);
    p.previous_focus = focus_;
    Widget& ref = *p.widget;
    ref.parent_ = nullptr;
    ref.host_ = this;
    ref.refresh_enabled_state();
    bool take_focus = p.options.take_focus;
    popups_.push_back(std::move(p));
    layout_popups();
    if (take_focus) {
        std::function<Widget*(Widget*)> first = [&](Widget* x) -> Widget* {
            if (x->visible() && x->enabled() && x->focusable()) return x;
            for (auto& c : x->children()) if (Widget* f = first(c.get())) return f;
            return nullptr;
        };
        if (Widget* f = first(&ref)) set_focus(f, false);
    }
    if (pointer_inside_ && !captured_) set_hovered(hit_test(pointer_));
    request_redraw();
    return ref;
}

bool WidgetHost::has_modal() const {
    return std::any_of(popups_.begin(), popups_.end(), [](auto& p) { return p.options.modal; });
}

bool WidgetHost::is_popup(const Widget* w) const {
    return std::any_of(popups_.begin(), popups_.end(), [&](auto& p) { return p.widget.get() == w; });
}

void WidgetHost::close_popup(Widget* w) {
    if (!is_popup(w)) return;
    // Popups opened from this one (submenus, a dropdown inside a dialog) go first.
    while (popups_.back().widget.get() != w) close_popup(popups_.back().widget.get());
    auto it = std::find_if(popups_.begin(), popups_.end(), [&](auto& p) { return p.widget.get() == w; });
    if (it == popups_.end()) return;
    Popup p = std::move(*it);
    popups_.erase(it);
    widget_removed(p.widget.get());
    // Return focus to where it was, if that widget still exists.
    auto attached = [&](Widget* x) {
        if (!x) return false;
        if (root_ && root_->is_ancestor_of(x)) return true;
        for (auto& q : popups_) if (q.widget->is_ancestor_of(x)) return true;
        return false;
    };
    if (p.options.take_focus && attached(p.previous_focus)) set_focus(p.previous_focus, false);
    if (pointer_inside_ && !captured_) set_hovered(hit_test(pointer_));
    request_redraw();
    if (p.options.on_close) p.options.on_close();
}

void WidgetHost::close_all_popups() {
    while (!popups_.empty()) close_popup(popups_.back().widget.get());
}

void WidgetHost::update_pressed() {
    // Pressed = button held on the captured widget *and* the pointer still over it
    // (dragging off a button un-presses it; dragging back re-presses it).
    if (captured_) captured_->set_state_flag(STATE_PRESSED, hovered_ == captured_);
}

void WidgetHost::pointer_left() {
    pointer_inside_ = false;
    if (!captured_) set_hovered(nullptr);
}

void WidgetHost::release_capture() {
    if (!captured_) return;
    captured_->set_state_flag(STATE_PRESSED, false);
    captured_ = nullptr;
    if (pointer_inside_) set_hovered(hit_test(pointer_)); else set_hovered(nullptr);
}

MouseCursor WidgetHost::cursor() const {
    Widget* w = captured_ ? captured_ : hovered_;
    return w ? w->cursor() : MouseCursor::Default;
}

// ---------------------------------------------------------------------------- focus

void WidgetHost::set_focus(Widget* w, bool visible) {
    if (w && (!w->focusable() || !w->enabled() || !w->visible())) return;
    if (w == focus_) {
        if (w) w->set_state_flag(STATE_FOCUS_VISIBLE, visible);
        return;
    }
    if (focus_) focus_->set_state_flag(STATE_FOCUSED | STATE_FOCUS_VISIBLE, false);
    focus_ = w;
    if (focus_) focus_->set_state_flag(visible ? (STATE_FOCUSED | STATE_FOCUS_VISIBLE) : STATE_FOCUSED, true);
    // Keyboard focus: let scroll containers bring the widget into view.
    if (focus_ && visible) focus_->scroll_into_view(focus_->local_rect());
}

bool WidgetHost::focus_next(bool backwards) {
    std::vector<Widget*> chain;   // depth-first visual order
    std::function<void(Widget*)> collect = [&](Widget* w) {
        if (!w->visible() || !w->enabled()) return;
        if (w->focusable()) chain.push_back(w);
        for (auto& c : w->children()) collect(c.get());
    };
    // A modal popup confines Tab traversal to itself.
    Widget* scope = root_.get();
    for (auto it = popups_.rbegin(); it != popups_.rend(); ++it)
        if (it->options.modal) { scope = it->widget.get(); break; }
    if (scope) collect(scope);
    if (chain.empty()) return false;
    size_t i = 0;
    auto it = std::find(chain.begin(), chain.end(), focus_);
    if (it == chain.end()) i = backwards ? chain.size() - 1 : 0;
    else {
        size_t cur = (size_t)(it - chain.begin());
        i = backwards ? (cur + chain.size() - 1) % chain.size() : (cur + 1) % chain.size();
    }
    set_focus(chain[i], true);
    return true;
}

// ---------------------------------------------------------------------------- events

bool WidgetHost::dispatch_pointer(Widget* target, const PointerEvent& e) {
    if (!target) return false;
    PointerEvent local = e;
    local.pos = target->from_window(e.pos);
    return target->on_pointer(local);
}

bool WidgetHost::handle_event(const Event& ev) {
    if (!root_) return false;

    if (auto* pe = std::get_if<PointerEvent>(&ev)) {
        using T = PointerEvent::Type;
        pointer_ = pe->pos;
        switch (pe->type) {
            case T::Enter:
            case T::Move: {
                pointer_inside_ = true;
                Widget* hit = hit_test(pe->pos);
                // While captured, only the captured widget can be "hovered".
                set_hovered(captured_ ? (hit == captured_ ? captured_ : nullptr) : hit);
                return dispatch_pointer(captured_ ? captured_ : hovered_, *pe);
            }
            case T::Leave:
                pointer_left();
                return false;
            case T::Down: {
                hide_tooltip();
                // A click outside the top popup closes it (and is consumed, like menus do). Submenu
                // chains close together; a click on a parent menu closes only what is above it.
                if (!captured_ && !popups_.empty() && !popups_.back().widget->bounds().contains(pe->pos)) {
                    bool closed = false, chained = false;
                    while (!popups_.empty()) {
                        Popup& top = popups_.back();
                        if (top.widget->bounds().contains(pe->pos)) break;
                        if (!top.options.dismiss_on_outside_click) {
                            if (top.options.modal) return true;   // blocked by the dialog
                            break;
                        }
                        chained = top.options.submenu;
                        close_popup(top.widget.get());
                        closed = true;
                        if (!chained) break;
                    }
                    bool into_parent = chained && !popups_.empty() && popups_.back().widget->bounds().contains(pe->pos);
                    if (closed && !into_parent) return true;
                }
                Widget* target = captured_ ? captured_ : hit_test(pe->pos);
                set_hovered(target);
                if (pe->button == PointerButton::Left && target) {
                    if (target->focusable()) set_focus(target, false);   // click focus: no ring
                    else if (focus_) focus_->set_state_flag(STATE_FOCUS_VISIBLE, false);
                    captured_ = target;
                    update_pressed();
                }
                // Bubble to interactive ancestors until someone handles it.
                bool handled = false;
                for (Widget* w = target; w && !handled; w = w->parent())
                    if ((w == target || w->accepts_pointer()) && dispatch_pointer(w, *pe)) handled = true;
                // Right button: the context menu of the widget under the pointer (after the widget
                // saw the press, so e.g. a list can select the row first).
                if (pe->button == PointerButton::Right && open_context_menu(hit_test(pe->pos, false), pe->pos)) return true;
                return handled;
            }
            case T::Up: {
                Widget* target = captured_ ? captured_ : hovered_;
                bool handled = dispatch_pointer(target, *pe);
                if (pe->button == PointerButton::Left && captured_) {
                    captured_->set_state_flag(STATE_PRESSED, false);
                    captured_ = nullptr;
                    set_hovered(hit_test(pe->pos));
                }
                return handled;
            }
        }
        return false;
    }

    if (auto* se = std::get_if<ScrollEvent>(&ev)) {
        for (Widget* w = hit_test(se->pos, false); w; w = w->parent()) {
            ScrollEvent local = *se;
            local.pos = w->from_window(se->pos);
            if (w->on_scroll(local)) return true;
        }
        return false;
    }

    if (auto* ke = std::get_if<KeyEvent>(&ev)) {
        if (ke->pressed) hide_tooltip();
        if (ke->pressed && ke->keysym == key::Escape && !popups_.empty() && popups_.back().options.close_on_escape) {
            close_popup(popups_.back().widget.get());
            return true;
        }
        if (ke->pressed && ke->keysym == key::Tab && !(ke->mods & (MOD_CTRL | MOD_ALT)))
            return focus_next(ke->mods & MOD_SHIFT);
        if (ke->pressed && ke->keysym == 0xfe20 /* ISO_Left_Tab (Shift+Tab) */) return focus_next(true);
        for (Widget* w = focus_; w; w = w->parent())
            if (w->on_key(*ke)) return true;
        if (!ke->pressed) return false;
        // Menu key / Shift+F10: context menu of the focused widget.
        constexpr uint32_t kMenuKey = 0xff67, kF10 = 0xffc7;
        if (focus_ && (ke->keysym == kMenuKey || (ke->keysym == kF10 && (ke->mods & MOD_SHIFT)))) {
            Rect a = focus_->keyboard_anchor();
            if (open_context_menu(focus_, focus_->to_window({a.x + 8, a.bottom()}))) return true;
        }
        if (has_modal()) return false;
        auto handlers = key_handlers_;   // handlers may add/remove handlers
        for (auto& [id, fn] : handlers)
            if (fn(*ke)) return true;
        return false;
    }

    if (auto* te = std::get_if<TextEvent>(&ev)) {
        for (Widget* w = focus_; w; w = w->parent())
            if (w->on_text(*te)) return true;
        return false;
    }
    return false;
}

// ---------------------------------------------------------------------------- shortcuts / context menus

uint64_t WidgetHost::add_key_handler(KeyHandler handler) {
    key_handlers_.push_back({++next_key_handler_, std::move(handler)});
    return next_key_handler_;
}

void WidgetHost::remove_key_handler(uint64_t id) {
    std::erase_if(key_handlers_, [id](auto& h) { return h.first == id; });
}

bool WidgetHost::open_context_menu(Widget* w, Point window_pos) {
    for (; w; w = w->parent()) {
        if (!w->enabled()) continue;
        if (std::shared_ptr<Menu> m = w->context_menu_at(w->from_window(window_pos))) {
            if (m->empty()) return false;
            popup_menu(*this, std::move(m), window_pos);
            return true;
        }
    }
    return false;
}

} // namespace uitk
