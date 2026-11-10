// ApplicationWindow: a ready-to-use top-level window = platform Window + renderer + WindowShell.
//
//   Application app;
//   ApplicationWindow win(app, {.title = "My app"});
//   auto& save = win.content().add<Button>("Save", ButtonStyle::Suggested);
//   save.clicked.connect([] { ... });
//   return app.run();
//
// Closing quits the application unless something is connected to close_requested.
#pragma once
#include <memory>

#include "uitk/core/application.h"
#include "uitk/core/signal.h"
#include "uitk/platform/window.h"
#include "uitk/render/renderer.h"
#include "uitk/widgets/window_shell.h"

namespace uitk {

class ApplicationWindow {
public:
    ApplicationWindow(Application& app, const WindowDesc& desc);
    ~ApplicationWindow();

    Window& window() { return *window_; }
    WindowRenderer& renderer() { return *renderer_; }
    WindowShell& shell() { return *shell_; }
    WidgetHost& host() { return shell_->host(); }
    HeaderBar& header_bar() { return shell_->header_bar(); }
    Box& content() { return shell_->content(); }

    void set_theme(const Theme& theme, bool animate = true) { shell_->set_theme(theme, animate); }
    const Theme& theme() const { return shell_->theme(); }

    Signal<> close_requested;                 // default (no slots): Application::quit()
    Signal<const KeyEvent&> key_pressed;      // key presses no widget handled (shortcuts)
    Signal<> before_paint;                    // each frame, before painting
    Signal<double> frame_painted;             // each frame: milliseconds spent drawing

private:
    Application& app_;
    std::unique_ptr<Window> window_;
    std::unique_ptr<WindowRenderer> renderer_;
    std::unique_ptr<WindowShell> shell_;
};

} // namespace uitk
