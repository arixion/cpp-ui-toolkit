// Widget: base class of every control and container.
//
// Extending: override what you need.
//   measure()          natural size for a given available width (layout pass 1)
//   layout()           position children inside size() (layout pass 2)
//   paint()            draw yourself in local coordinates (children are drawn after)
//   paint_over()       draw after children (overlays, focus rings)
//   on_pointer/...     input in local coordinates; return true when handled
//   on_state_changed() react to hover/press/focus/disabled changes (e.g. StateLayer::update)
// The WidgetHost owns event routing, hover tracking, pointer capture, focus and redraws.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "uitk/core/animation.h"
#include "uitk/core/event.h"
#include "uitk/core/geometry.h"
#include "uitk/platform/window.h"
#include "uitk/render/canvas.h"
#include "uitk/widgets/state.h"
#include "uitk/widgets/theme.h"

namespace uitk {

class WidgetHost;
class Menu;

enum class Orientation { Horizontal, Vertical };
enum class Align { Fill, Start, Center, End };

// Everything a widget needs while painting one frame.
struct PaintContext {
    Canvas&      canvas;
    FrameClock&  clock;
    const Theme& theme;          // sampled for this frame (already cross-faded)
    float        window_active;  // 1 = focused window, 0 = backdrop (animated)

    // Content colour that dims when the window is in the backdrop (titles, header icons).
    Color backdrop(Color active_color) const { return lerp(theme.foreground_dim, active_color, window_active); }
};

class Widget {
public:
    Widget() = default;
    virtual ~Widget() = default;
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;

    // ---- tree ----
    template <class W, class... Args>
    W& add(Args&&... args) {
        return static_cast<W&>(add_child(std::make_unique<W>(std::forward<Args>(args)...)));
    }
    Widget& add_child(std::unique_ptr<Widget> child);
    Widget& insert_child(size_t index, std::unique_ptr<Widget> child);
    template <class W, class... Args>
    W& insert(size_t index, Args&&... args) {
        return static_cast<W&>(insert_child(index, std::make_unique<W>(std::forward<Args>(args)...)));
    }
    std::unique_ptr<Widget> remove_child(Widget* child);
    void clear_children();
    Widget* parent() const { return parent_; }
    const std::vector<std::unique_ptr<Widget>>& children() const { return children_; }
    WidgetHost* host() const;
    bool is_ancestor_of(const Widget* w) const;

    // ---- geometry (logical px) ----
    const Rect& bounds() const { return bounds_; }       // in parent coordinates
    void set_bounds(const Rect& r);
    Size size() const { return bounds_.size(); }
    Rect local_rect() const { return Rect::from_size(bounds_.size()); }
    Point to_window(Point local) const;
    Point from_window(Point window_point) const;

    // ---- layout properties ----
    // Setters are no-ops when the value is unchanged (no needless layouts or frames).
    void set_expand(bool horizontal, bool vertical) {
        if (horizontal == hexpand_ && vertical == vexpand_) return;
        hexpand_ = horizontal; vexpand_ = vertical; invalidate_layout();
    }
    bool hexpand() const { return hexpand_; }
    bool vexpand() const { return vexpand_; }
    void set_align(Align horizontal, Align vertical) {
        if (horizontal == halign_ && vertical == valign_) return;
        halign_ = horizontal; valign_ = vertical; invalidate_layout();
    }
    Align halign() const { return halign_; }
    Align valign() const { return valign_; }
    void set_min_size(Size s) { if (s != min_size_) { min_size_ = s; invalidate_layout(); } }
    Size min_size() const { return min_size_; }
    // Exact size, overriding measure() (0 in a dimension = natural). E.g. compact icon buttons.
    void set_fixed_size(Size s) { if (s != fixed_size_) { fixed_size_ = s; invalidate_layout(); } }
    Size fixed_size() const { return fixed_size_; }

    // ---- layout ----
    // Natural size when given `available_width` (wrapping content uses it). Default: min_size().
    virtual Size measure(float available_width, const Theme& theme);
    // Position children inside size(). Default: every child fills this widget.
    virtual void layout(const Theme& theme);
    // What layouts use: measure(), raised to min_size(), replaced by fixed_size() where set.
    Size measure_clamped(float available_width, const Theme& theme);

    // ---- visibility / state ----
    void set_visible(bool v);
    bool visible() const { return visible_; }
    void set_enabled(bool e);
    bool enabled() const;                 // false if this or any ancestor is disabled
    uint32_t state() const { return state_; }
    bool has_focus() const { return state_ & STATE_FOCUSED; }
    void grab_focus();

    // Tooltip shown after hovering for a moment (empty = none).
    void set_tooltip(std::string text) { tooltip_ = std::move(text); }
    const std::string& tooltip() const { return tooltip_; }

    // Ask scroll containers above this widget to bring `area` (local coordinates) into view.
    // Called automatically for keyboard focus; lists call it for their cursor row.
    void scroll_into_view(const Rect& area);
    // Scroll containers override this: `from` (a descendant) wants `area` (in its coordinates) visible.
    virtual void reveal_area(Widget* /*from*/, const Rect& /*area*/) {}
    // The part of this widget not clipped away by scrolling ancestors (local coordinates).
    // Virtualized widgets paint only this.
    Rect visible_rect() const;

    virtual bool accepts_pointer() const { return false; }   // interactive: gets hover/press
    virtual bool focusable() const { return false; }         // in the Tab chain
    virtual MouseCursor cursor() const { return MouseCursor::Default; }
    bool clips_children() const { return clip_children_; }
    void set_clip_children(bool c) { clip_children_ = c; }

    // ---- context menu (right-click, the Menu key or Shift+F10) ----
    // The host looks for a menu from the widget under the pointer up through its ancestors.
    void set_context_menu(std::shared_ptr<Menu> menu) { context_menu_ = std::move(menu); }
    // Override to build a menu for a position (e.g. for the row under the pointer).
    virtual std::shared_ptr<Menu> context_menu_at(Point /*local*/) { return context_menu_; }
    // Where a keyboard-opened context menu appears (local coordinates), e.g. the current row.
    virtual Rect keyboard_anchor() const { return local_rect(); }

    // ---- painting ----
    virtual void paint(PaintContext&) {}
    virtual void paint_over(PaintContext&) {}
    void invalidate();          // request a repaint
    void invalidate_layout();   // request a layout pass (and a repaint); drops cached measurements
                                // of this widget and its ancestors

    // ---- input (local coordinates) ----
    virtual bool on_pointer(const PointerEvent&) { return false; }
    virtual bool on_scroll(const ScrollEvent&) { return false; }
    virtual bool on_key(const KeyEvent&) { return false; }
    virtual bool on_text(const TextEvent&) { return false; }

protected:
    virtual void on_state_changed(uint32_t /*old_state*/) {}
    // Selectable widgets (list rows, toggles, check boxes) own their CHECKED flag; all other
    // interaction flags are managed by the WidgetHost.
    void set_checked_state(bool on) { set_state_flag(STATE_CHECKED, on); }
    // Theme in effect (target theme; paint() receives the per-frame sampled one).
    const Theme& theme() const;

private:
    friend class WidgetHost;
    void set_state_flag(uint32_t flag, bool on);
    void refresh_enabled_state();   // after an ancestor's enabled state changed

    Widget* parent_ = nullptr;
    WidgetHost* host_ = nullptr;    // set on the root only
    std::vector<std::unique_ptr<Widget>> children_;
    Rect bounds_;
    Size min_size_;
    Size fixed_size_;
    std::string tooltip_;
    std::shared_ptr<Menu> context_menu_;
    // Measurement cache (two entries: rows measure children unbounded and at their allocation).
    struct MeasureEntry { float available = -1; Size size; };
    MeasureEntry measure_cache_[2];
    int measure_cache_next_ = 0;
    const Theme* measure_theme_ = nullptr;
    void drop_measure_cache_recursive();
    bool hexpand_ = false, vexpand_ = false;
    Align halign_ = Align::Fill, valign_ = Align::Fill;
    bool visible_ = true, enabled_ = true, clip_children_ = false;
    uint32_t state_ = STATE_NONE;
};

// Fills free space in a Box (pushes siblings apart).
class Spacer : public Widget {
public:
    Spacer() { set_expand(true, true); }
};

} // namespace uitk
