#include "uitk/widgets/window_shell.h"

namespace uitk {

WindowShell::WindowShell(Window* window, std::string title)
    : window_(window), host_(window), frame_(window) {
    Box& root = host_.emplace_root<Box>(Orientation::Vertical, 0.f);
    header_ = &root.add<HeaderBar>(std::move(title));
    content_ = &root.add<Box>(Orientation::Vertical, 0.f);
    content_->set_expand(true, true);
    header_->close_requested.connect([this] { close_requested.emit(); });
}

void WindowShell::set_maximized(bool m) {
    frame_.set_maximized(m);
    header_->set_maximized(m);
    host_.request_layout();
}

void WindowShell::set_active(bool a) { host_.set_window_active(a, false); }

bool WindowShell::handle_event(const Event& ev) {
    if (auto* st = std::get_if<StateEvent>(&ev)) {
        frame_.set_maximized(st->maximized);
        header_->set_maximized(st->maximized);
        host_.request_layout();
        return false;
    }
    if (auto* f = std::get_if<FocusEvent>(&ev)) { host_.set_window_active(f->focused); return false; }
    if (std::holds_alternative<CloseEvent>(ev)) { close_requested.emit(); return true; }
    if (std::holds_alternative<ResizeEvent>(ev)) { host_.request_layout(); return false; }

    if (auto* pe = std::get_if<PointerEvent>(&ev); pe && window_) {
        using T = PointerEvent::Type;
        // Resize edges take precedence over widgets (unless a widget holds the pointer).
        bool probe = pe->type == T::Enter || pe->type == T::Move || pe->type == T::Down;
        if (probe && !host_.has_capture()) {
            unsigned edges = frame_.resize_edges(pe->pos, window_->size(), host_.theme());
            if (edges) {
                host_.pointer_left();
                window_->set_cursor(WindowFrame::cursor_for_edges(edges));
                if (pe->type == T::Down && pe->button == PointerButton::Left) window_->begin_resize(edges);
                return true;
            }
        }
        bool handled = host_.handle_event(ev);
        window_->set_cursor(host_.cursor());
        return handled;
    }
    return host_.handle_event(ev);
}

void WindowShell::paint(Canvas& c, Size size) {
    host_.layout(size);
    host_.prepare_frame();   // sample the animated theme / backdrop once for frame + widgets
    frame_.begin(c, size, host_.frame_theme(), host_.frame_window_active());
    host_.paint(c);
    frame_.end(c);
}

} // namespace uitk
