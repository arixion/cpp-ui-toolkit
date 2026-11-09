// MenuList: a popup list of choices (used by Dropdown; the base for menus later).
// Dropdown: a select box showing the current choice; opens a MenuList popup.
#pragma once
#include <string>
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/widget.h"

namespace uitk {

class MenuList : public Widget {
public:
    MenuList(std::vector<std::string> items, int checked = -1);

    Signal<int> activated;   // click or Enter on an item (the popup closes itself)

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;

private:
    int item_at(float y) const;
    void activate(int index);
    float row_height(const Theme& t) const { return t.control_height - 2; }

    std::vector<std::string> items_;
    int checked_;
    int highlighted_;
    static constexpr float kPad = 4;
};

class Dropdown : public Widget {
public:
    explicit Dropdown(std::vector<std::string> items = {}, int selected = 0);

    Signal<int> changed;
    void set_items(std::vector<std::string> items);
    void set_selected(int index);
    int selected() const { return selected_; }
    const std::string& selected_text() const;
    void open();
    bool is_open() const { return popup_ != nullptr; }

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    void paint_over(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;

protected:
    void on_state_changed(uint32_t) override { layer_.update(state() | (popup_ ? STATE_PRESSED : 0u), theme()); }

private:
    std::vector<std::string> items_;
    int selected_;
    Widget* popup_ = nullptr;
    StateLayer layer_;
};

} // namespace uitk
