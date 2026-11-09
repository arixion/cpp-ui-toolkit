#include "uitk/widgets/dialog.h"

#include <algorithm>

#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/icon.h"

namespace uitk {

namespace {
constexpr uint32_t kKpEnter = 0xff8d;
}

Dialog::Dialog(std::string title) : Frame(FrameStyle::Card, Orientation::Vertical, 0.f) {
    header_ = &add<Box>(Orientation::Horizontal, 8.f);
    header_->set_padding(Insets{16, 8, 8, 4});
    title_ = &header_->add<Label>(title, TextRole::Button);
    title_->set_align(Align::Start, Align::Center);
    title_->set_ellipsize(true);
    title_->set_expand(true, false);
    Button& close = header_->add<Button>(glyph(Glyph::Close), ButtonStyle::Flat);
    close.set_tooltip("Close (Esc)");
    close.set_focusable(false);
    close.clicked.connect([this] { respond(cancel_response_); });
    sep_top_ = &add<Separator>();
    sep_top_->set_visible(false);

    body_ = &add<Box>(Orientation::Vertical, 10.f);
    body_->set_padding(Insets{16, 8, 16, 12});
    body_->set_expand(true, true);

    sep_bottom_ = &add<Separator>();
    sep_bottom_->set_visible(false);
    buttons_ = &add<Box>(Orientation::Horizontal, 8.f);
    buttons_->set_padding(Insets{16, 10, 12, 12});
    buttons_->set_justify(Justify::End);
    buttons_->set_visible(false);
    if (title.empty()) title_->set_visible(false);
}

void Dialog::set_title(std::string t) {
    title_->set_visible(!t.empty());
    title_->set_text(std::move(t));
}

void Dialog::set_section_separators(bool on) {
    sep_top_->set_visible(on);
    sep_bottom_->set_visible(on && buttons_->visible());
}

Button& Dialog::add_button(std::string label, int response, ButtonStyle style) {
    Button& b = buttons_->add<Button>(std::move(label), style);
    b.set_min_size({80, 0});
    b.clicked.connect([this, response] { respond(response); });
    response_buttons_.push_back({response, &b});
    buttons_->set_visible(true);
    if (sep_top_->visible()) sep_bottom_->set_visible(true);
    return b;
}

Button* Dialog::button_for(int response) const {
    for (auto& [r, b] : response_buttons_) if (r == response) return b;
    return nullptr;
}

void Dialog::respond(int response) {
    if (responded_) return;
    responded_ = true;
    response_ = response;
    Signal<int> sig = responded;   // the dialog is destroyed when its popup closes
    if (WidgetHost* h = host()) h->close_popup(this);
    sig.emit(response);
}

Size Dialog::measure(float available_width, const Theme& t) {
    if (preferred_.width > 0) return preferred_;
    float max_w = std::min(available_width, 560.f);
    Size nat = Frame::measure(max_w, t);
    float w = std::clamp(nat.width, std::min(360.f, max_w), max_w);
    return {w, Frame::measure(w, t).height};
}

bool Dialog::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    if ((e.keysym == key::Return || e.keysym == kKpEnter) && default_response_ != None) {
        Button* b = button_for(default_response_);
        if (!b || b->enabled()) { respond(default_response_); return true; }
    }
    return Frame::on_key(e);
}

void Dialog::present(WidgetHost& host, std::unique_ptr<Dialog> dialog) {
    WidgetHost::PopupOptions po;
    po.modal = true;
    po.centered = true;
    po.dismiss_on_outside_click = false;
    po.close_on_escape = true;
    Dialog* d = dialog.get();
    // Closed without a response (Escape): report the cancel response. The dialog is still
    // alive while on_close runs.
    po.on_close = [d] {
        if (d->responded_) return;
        d->responded_ = true;
        d->response_ = d->cancel_response_;
        d->responded.emit(d->cancel_response_);
    };
    host.show_popup(std::move(dialog), {}, std::move(po));
    // Keyboard users land on the default button when nothing in the body takes focus.
    if (Button* b = d->button_for(d->default_response_); b && host.focus() && !d->body_->is_ancestor_of(host.focus()))
        host.set_focus(b, false);
}

Dialog& Dialog::message(WidgetHost& host, MessageKind kind, std::string heading, std::string text,
                        std::vector<DialogButton> buttons, std::function<void(int)> on_response) {
    auto d = std::make_unique<Dialog>();
    Box& row = d->body().add<Box>(Orientation::Horizontal, 14.f);
    row.set_padding(Insets{4, 10, 4, 0});
    if (kind != MessageKind::None) {
        Glyph g = kind == MessageKind::Warning ? Glyph::Warning : kind == MessageKind::Error ? Glyph::Error : Glyph::Info;
        Icon& ic = row.add<Icon>(glyph(g), 32);
        ic.set_token(kind == MessageKind::Error ? ColorToken::Error : ColorToken::Accent);
        ic.set_align(Align::Start, Align::Start);
    }
    Box& texts = row.add<Box>(Orientation::Vertical, 6.f);
    texts.set_expand(true, false);
    Label& h = texts.add<Label>(std::move(heading), TextRole::Button);
    h.set_wrap(true);
    if (!text.empty()) {
        Label& l = texts.add<Label>(std::move(text), TextRole::Dim);
        l.set_wrap(true);
    }
    int def = Dialog::None;
    for (auto& b : buttons) {
        d->add_button(b.label, b.response, b.style);
        if (b.style == ButtonStyle::Primary || b.style == ButtonStyle::Destructive) def = b.response;
    }
    if (def == Dialog::None && !buttons.empty()) def = buttons.back().response;
    d->set_default_response(def);
    // With a single button, Escape answers with it too ("OK" is also the cancel).
    if (buttons.size() == 1) d->set_cancel_response(buttons[0].response);
    if (on_response) d->responded.connect(std::move(on_response));
    Dialog& ref = *d;
    present(host, std::move(d));
    return ref;
}

} // namespace uitk
