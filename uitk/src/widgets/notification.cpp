#include "uitk/widgets/notification.h"

#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/icons.h"

namespace uitk {

Notification::Notification(std::string message, std::string source, Image icon)
    : Frame(FrameStyle::Card, Orientation::Vertical, 6.f) {
    set_padding(Insets{12, 8, 8, 8});

    Box& top = add<Box>(Orientation::Horizontal, 8.f);
    Icon& ic = top.add<Icon>(icon.valid() ? std::move(icon) : glyph(Glyph::Info));
    ic.set_align(Align::Start, Align::Start);
    ic.set_min_size({16, 20});
    Label& msg = top.add<Label>(std::move(message));
    msg.set_wrap(true);
    msg.set_expand(true, false);
    msg.set_align(Align::Fill, Align::Start);
    settings_ = &top.add<Button>(icons::symbolic("emblem-system"), ButtonStyle::Flat);
    settings_->set_align(Align::Start, Align::Start);
    Button& close = top.add<Button>(glyph(Glyph::Close), ButtonStyle::Flat);
    close.set_align(Align::Start, Align::Start);
    close.clicked.connect([this] { closed.emit(); });

    Box& bottom = add<Box>(Orientation::Horizontal, 6.f);
    bottom.set_padding(Insets{24, 0, 0, 0});   // align with the message text
    if (!source.empty()) {
        Label& src = bottom.add<Label>("Source: " + source, TextRole::Dim);
        src.set_ellipsize(true);
        src.set_expand(true, false);
        src.set_align(Align::Fill, Align::Center);
    } else {
        bottom.add<Spacer>().set_expand(true, false);
    }
    actions_ = &bottom.add<Box>(Orientation::Horizontal, 6.f);
    actions_->set_align(Align::End, Align::Center);
}

Button& Notification::add_action(std::string label, ButtonStyle style) {
    return actions_->add<Button>(std::move(label), style);
}

} // namespace uitk
