// Button: label and/or icon, several styles, full interaction states.
//
//   auto& b = box.add<Button>("Save", ButtonStyle::Primary);
//   b.clicked.connect([] { ... });
//
// Every style is resolved from theme tokens (ButtonLook), and states (hover, press, keyboard
// focus ring, disabled) come from the shared StateLayer, so all buttons look and animate
// consistently in any theme. Keyboard: Space activates on release, Enter on press.
#pragma once
#include <optional>
#include <string>

#include "uitk/core/signal.h"
#include "uitk/widgets/widget.h"

namespace uitk {

enum class ButtonStyle {
    Secondary,       // standard button (VS Code "secondary", Adwaita neutral)
    Primary,         // the main action (accent fill)
    Destructive,     // delete / discard
    Flat,            // transparent until hovered: toolbars, header bars, icon actions
    Circular,        // round icon button
    WindowControl,   // minimize / maximize / close (shape from Theme::window_controls)
    StatusItem,      // status bar entry (small, chrome-coloured, square)
    Link,            // hyperlink: accent text, underline on hover, hand cursor
    Tab,             // tab in a TabView strip
};

// Everything that defines how a button style looks in a theme. One function maps
// (style, theme) -> look; new styles or themes only touch data.
struct ButtonLook {
    ControlColors colors;
    float radius = 0;          // ignored when `circle`
    bool  circle = false;
    float height = 0;          // natural height
    float padding_x = 0;
    float icon_only_width = 0; // square/fixed width for icon-only buttons
    TextRole text_role = TextRole::Button;
    float text_size = 0;       // 0 = role size
};
ButtonLook button_look(ButtonStyle style, const Theme& theme, bool icon_only);

class Button : public Widget {
public:
    explicit Button(std::string label = {}, ButtonStyle style = ButtonStyle::Secondary);
    explicit Button(Image icon, ButtonStyle style = ButtonStyle::Flat);

    Signal<> clicked;

    void set_label(std::string label);
    const std::string& label() const { return label_; }
    void set_icon(Image icon);
    const Image& icon() const { return icon_; }
    void set_icon_size(float px) { if (px != icon_size_) { icon_size_ = px; invalidate_layout(); } }   // 0 = theme size
    float icon_size() const { return icon_size_; }
    void set_style(ButtonStyle s) { if (s != style_) { style_ = s; invalidate_layout(); } }
    ButtonStyle style() const { return style_; }
    // Override the corner radii (grouped/split buttons round only their outer corners).
    void set_corner_radii(std::optional<CornerRadii> r) { if (r != corners_) { corners_ = r; invalidate(); } }
    void set_focusable(bool f) { focusable_ = f; }
    void set_backdrop_dim(bool d) { if (d != backdrop_dim_) { backdrop_dim_ = d; invalidate(); } }   // dim content in inactive windows

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return focusable_; }
    MouseCursor cursor() const override { return style_ == ButtonStyle::Link ? MouseCursor::Pointer : MouseCursor::Default; }

    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    void paint_over(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;

protected:
    void on_state_changed(uint32_t old_state) override;
    // The resolved look; subclasses adjust it (e.g. checked toggle buttons).
    virtual ButtonLook look(const Theme& t) const { return button_look(style_, t, icon_.valid() && label_.empty()); }
    const StateLayer& layer() const { return layer_; }

private:
    uint32_t visual_state() const;   // widget state + keyboard press
    float icon_px(const Theme& t) const;
    CornerRadii radii(const ButtonLook& l) const;

    std::string label_;
    Image icon_;
    ButtonStyle style_;
    StateLayer layer_;
    std::optional<CornerRadii> corners_;
    float icon_size_ = 0;
    bool focusable_ = true;
    bool backdrop_dim_ = false;
    bool key_pressed_ = false;   // Space held while focused
};

} // namespace uitk
