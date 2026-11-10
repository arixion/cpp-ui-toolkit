// StatusBar: a compact strip of clickable items, left- and right-aligned (VS Code style).
#pragma once
#include <string>

#include "uitk/widgets/box.h"
#include "uitk/widgets/button.h"

namespace uitk {

class StatusBar : public Box {
public:
    StatusBar();
    // Items are Buttons (ButtonStyle::StatusItem): hover feedback, clicked signal, optional icon.
    Button& add_start(std::string text, Image icon = {});   // left group
    Button& add_end(std::string text, Image icon = {});     // right group

    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;

private:
    Button& make(size_t index, std::string text, Image icon);
    Widget* spacer_;
};

} // namespace uitk
