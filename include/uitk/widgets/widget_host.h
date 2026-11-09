// WidgetHost: connects a widget tree to a window (or to an offscreen renderer).
//
// Owns everything controls should not re-implement: hit testing, hover enter/leave, pointer
// capture and the pressed state, keyboard focus (click focus, Tab/Shift+Tab traversal, the
// "focus visible" ring rule), event bubbling, layout scheduling, the animated theme and the
// window's active/backdrop state.
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "uitk/core/animation.h"
#include "uitk/core/event.h"
#include "uitk/platform/window.h"
#include "uitk/render/canvas.h"
#include "uitk/widgets/theme.h"
#include "uitk/widgets/widget.h"

namespace uitk {

class WidgetHost {
public:
    explicit WidgetHost(Window* window = nullptr);   // null: offscreen (renders final states)
    ~WidgetHost();

    // ---- tree ----
    Widget& set_root(std::unique_ptr<Widget> root);
    template <class W, class... Args>
    W& emplace_root(Args&&... args) {
        return static_cast<W&>(set_root(std::make_unique<W>(std::forward<Args>(args)...)));
    }
    Widget* root() const { return root_.get(); }
    Window* window() const { return window_; }

    // ---- theme ----
    void set_theme(const Theme& theme, bool animate = true);
    const Theme& theme() const { return theme_.target(); }

    // ---- frame ----
    FrameClock& clock();
    void set_frame_clock(FrameClock* clock) { external_clock_ = clock; }   // offscreen animations
    void set_window_active(bool active, bool animate = true);
    void layout(Size size);       // lays out if the size changed or a layout was requested
    void prepare_frame();         // samples the animated theme / backdrop for this frame
    void paint(Canvas& canvas);   // prepare_frame() + paint the tree
    const Theme& frame_theme() const { return frame_theme_; }
    float frame_window_active() const { return frame_active_; }

    // ---- input ----
    bool handle_event(const Event& e);   // true if a widget consumed it
    void pointer_left();                  // the pointer moved onto something we don't own
    MouseCursor cursor() const;
    Widget* hovered() const { return hovered_; }
    Widget* focus() const { return focus_; }

    // Drop pointer capture without delivering an Up (e.g. the compositor took over the pointer
    // for an interactive move/resize, so the release will never reach us).
    void release_capture();
    bool has_capture() const { return captured_ != nullptr; }

    void set_focus(Widget* w, bool visible);   // visible = show the focus ring (keyboard focus)
    bool focus_next(bool backwards);

    // ---- popups (dropdown lists, menus, tooltips): an in-window layer above the tree ----
    struct PopupOptions {
        bool dismiss_on_outside_click = true;   // click outside / Escape closes it
        bool take_focus = true;                 // move keyboard focus into it (restored on close)
        bool match_anchor_width = false;        // at least as wide as the anchor
        bool close_on_escape = true;
        bool modal = false;      // dims the window and blocks input outside the popup (dialogs)
        bool centered = false;   // centre in the window instead of anchoring
        enum class Placement {
            Below,   // under the anchor (flips above when there is no room): dropdowns, menus
            Right,   // beside the anchor's right edge (flips left): submenus
        } placement = Placement::Below;
        float gap = 4;           // distance from the anchor
        // Part of the popup below it (a submenu): a click outside closes the whole chain, and a
        // click on the parent popup closes only the popups above it and reaches the parent.
        bool submenu = false;
        std::function<void()> on_close;
    };
    // `anchor` in window coordinates; the popup opens below it (above if there is no room) and
    // is kept inside the window.
    Widget& show_popup(std::unique_ptr<Widget> popup, Rect anchor, PopupOptions options);
    template <class W, class... Args>
    W& emplace_popup(Rect anchor, PopupOptions options, Args&&... args) {
        return static_cast<W&>(show_popup(std::make_unique<W>(std::forward<Args>(args)...), anchor, std::move(options)));
    }
    // Closes the popup and every popup opened above it (submenus, dropdowns inside a dialog).
    void close_popup(Widget* popup);
    void close_all_popups();
    bool has_popup() const { return !popups_.empty(); }
    bool has_modal() const;
    bool is_popup(const Widget* w) const;
    Widget* top_popup() const { return popups_.empty() ? nullptr : popups_.back().widget.get(); }

    // ---- application keyboard shortcuts ----
    // Handlers see key presses that no focused widget consumed (menu shortcuts, F-keys...).
    // They are not called while a modal popup is open. Return true to consume the key.
    using KeyHandler = std::function<bool(const KeyEvent&)>;
    uint64_t add_key_handler(KeyHandler handler);
    void remove_key_handler(uint64_t id);

    // Opens the context menu for `w` (Widget::context_menu_at) at `window_pos`. True if one opened.
    bool open_context_menu(Widget* w, Point window_pos);

    // Tooltips: shown automatically for Widget::tooltip() after a hover delay.
    void show_tooltip(Widget* for_widget, Point window_pos);   // immediately (tests/snapshots)
    void hide_tooltip();
    static constexpr int TOOLTIP_DELAY_MS = 600;

    // ---- invalidation ----
    void request_redraw();
    void request_layout();
    void widget_removed(Widget* subtree);   // called before a subtree is detached/hidden/disabled

private:
    Widget* hit_test(Point window_point, bool interactive_only = true) const;
    Widget* hit_test_tree(Widget* root, Point window_point, bool interactive_only = true) const;
    void layout_popups();
    void schedule_tooltip(Widget* w);
    void set_hovered(Widget* w);
    void update_pressed();
    bool dispatch_pointer(Widget* target, const PointerEvent& e);
    void paint_widget(Widget* w, PaintContext& ctx);

    Window* window_;
    std::unique_ptr<Widget> root_;
    Animated<Theme> theme_{Theme::light()};
    Animated<float> active_{1.f};
    Theme frame_theme_ = Theme::light();
    float frame_active_ = 1.f;
    FrameClock settled_clock_ = FrameClock::settled();
    FrameClock* external_clock_ = nullptr;

    Widget* hovered_ = nullptr;
    Widget* captured_ = nullptr;   // receives pointer events while a button is held
    Widget* focus_ = nullptr;
    Point   pointer_{-1e9f, -1e9f};
    bool    pointer_inside_ = false;
    bool    layout_dirty_ = true;
    bool    in_layout_ = false;
    Size    laid_out_size_{-1, -1};

    struct Popup {
        std::unique_ptr<Widget> widget;
        Rect anchor;
        PopupOptions options;
        Widget* previous_focus = nullptr;
    };
    std::vector<Popup> popups_;
    std::unique_ptr<Widget> tooltip_;
    Widget* tooltip_owner_ = nullptr;
    uint64_t tooltip_timer_ = 0;
    std::vector<std::pair<uint64_t, KeyHandler>> key_handlers_;
    uint64_t next_key_handler_ = 0;
};

} // namespace uitk
