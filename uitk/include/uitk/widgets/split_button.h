// SplitButton: a main action plus a dropdown chevron ("New Profile | v"), built from two Buttons
// that share one rounded outline, with a divider between them.
#pragma once
#include <string>

#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/button.h"

namespace uitk {

class SplitButton : public Box {
public:
    explicit SplitButton(std::string label, ButtonStyle style = ButtonStyle::Primary);

    Signal<> clicked;          // main part
    Signal<> menu_requested;   // chevron part

    Button& main_button() { return *main_; }
    Button& menu_button() { return *menu_; }

    void layout(const Theme& theme) override;
    void paint_over(PaintContext& ctx) override;

private:
    Button* main_;
    Button* menu_;
    ButtonStyle style_;
};

} // namespace uitk
