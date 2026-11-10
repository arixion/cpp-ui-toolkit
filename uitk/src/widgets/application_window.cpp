#include "uitk/widgets/application_window.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace uitk {

ApplicationWindow::ApplicationWindow(Application& app, const WindowDesc& desc) : app_(app) {
    window_ = Window::create(app, desc);
    if (!window_) { fprintf(stderr, "uitk: no window system available\n"); std::exit(1); }
    renderer_ = WindowRenderer::create(*window_);
    shell_ = std::make_unique<WindowShell>(window_.get(), desc.title);

    shell_->close_requested.connect([this] {
        if (close_requested.empty()) app_.quit();
        else close_requested.emit();
    });

    window_->on_event = [this](const Event& e) {
        if (shell_->handle_event(e)) return;
        if (auto* k = std::get_if<KeyEvent>(&e); k && k->pressed) key_pressed.emit(*k);
    };

    window_->on_frame = [this] {
        auto t0 = std::chrono::steady_clock::now();
        Canvas* c = renderer_->begin_frame();
        if (!c) return;
        before_paint.emit();
        shell_->paint(*c, window_->size());
        renderer_->end_frame();
        frame_painted.emit(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    };
    window_->request_redraw();
}

ApplicationWindow::~ApplicationWindow() {
    if (window_) { window_->on_event = nullptr; window_->on_frame = nullptr; }
    shell_.reset();      // widgets first
    renderer_.reset();   // then the GPU/CPU surface
}

} // namespace uitk
