// HeaderBar: the title bar as a widget.
//   - start()/end() boxes for application buttons (like GTK's pack_start/pack_end)
//   - title: plain centred text, or the VS Code "command center" box (Theme::title_style)
//   - window controls: ordinary Buttons with ButtonStyle::WindowControl; their shape and icons
//     follow Theme::window_controls (flat line glyphs / round Adwaita icons)
//   - caption behaviour: drag to move, double-click to maximize, right-click for the window menu
#pragma once
#include <string>

#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/button.h"
#include "uitk/widgets/label.h"

namespace uitk {

class HeaderBar : public Widget {
public:
    explicit HeaderBar(std::string title = {});

    void set_title(std::string t) { title_->set_text(std::move(t)); }
    Label& title_label() { return *title_; }
    Box& start() { return *start_; }   // left side, for app buttons
    Box& end() { return *end_; }       // right side, before the window controls

    void set_show_window_controls(bool show);
    void set_maximized(bool maximized);   // swaps the maximize/restore icon
    Button& minimize_button() { return *min_; }
    Button& maximize_button() { return *max_; }
    Button& close_button() { return *close_; }

    Signal<> close_requested;

    bool accepts_pointer() const override { return true; }   // caption: move/maximize/menu
    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;

private:
    void apply_control_style(const Theme& t);

    Label* title_;
    Box* start_;
    Box* end_;
    Box* controls_;
    Button *min_, *max_, *close_;
    bool maximized_ = false;
    int control_style_ = -1;   // WindowControlsStyle currently applied
    Rect title_box_;           // command-center box (TitleStyle::CommandCenter)
};

} // namespace uitk
