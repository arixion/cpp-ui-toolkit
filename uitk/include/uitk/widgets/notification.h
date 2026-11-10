// Notification: a toast card (VS Code style): icon, message, source line, action buttons,
// settings and close buttons. Composed entirely from Frame, Icon, Label and Button.
#pragma once
#include <string>

#include "uitk/core/signal.h"
#include "uitk/widgets/button.h"
#include "uitk/widgets/frame.h"
#include "uitk/widgets/icon.h"
#include "uitk/widgets/label.h"

namespace uitk {

class Notification : public Frame {
public:
    Notification(std::string message, std::string source = {}, Image icon = {});

    // Actions on the right of the source line; the first is usually Primary.
    Button& add_action(std::string label, ButtonStyle style = ButtonStyle::Secondary);
    Button& settings_button() { return *settings_; }

    Signal<> closed;   // close button (the owner decides whether to remove the card)

private:
    Box* actions_;
    Button* settings_;
};

} // namespace uitk
