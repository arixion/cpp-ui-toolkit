// WindowShell: a complete client-side-decorated window body = frame + header bar + content,
// driven by a WidgetHost. Used by ApplicationWindow (on screen) and directly for offscreen
// rendering (snapshots, thumbnails), so both paths run exactly the same code.
#pragma once
#include <string>

#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/header_bar.h"
#include "uitk/widgets/widget_host.h"
#include "uitk/widgets/window_frame.h"

namespace uitk {

class WindowShell {
public:
    WindowShell(Window* window, std::string title);   // window may be null (offscreen)

    WidgetHost& host() { return host_; }
    WindowFrame& frame() { return frame_; }
    HeaderBar& header_bar() { return *header_; }
    Box& content() { return *content_; }   // vertical box filling the area below the header bar

    Signal<> close_requested;   // close button or the compositor's close request

    void set_theme(const Theme& theme, bool animate = true) { host_.set_theme(theme, animate); }
    const Theme& theme() const { return host_.theme(); }

    // Routes window events: frame (state, focus, resize edges) first, then the widgets.
    bool handle_event(const Event& e);
    // Lays out and paints the whole window into `c` (origin = window's top-left).
    void paint(Canvas& c, Size window_size);

    // Offscreen use: state that a real window would report through events.
    void set_maximized(bool maximized);
    void set_active(bool active);

private:
    Window* window_;
    WidgetHost host_;
    WindowFrame frame_;
    HeaderBar* header_;
    Box* content_;
};

} // namespace uitk
