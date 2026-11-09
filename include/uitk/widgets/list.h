// ListBox / ListRow: a vertical list of selectable rows (VS Code-style lists and trees).
//
//   auto& list = panel.add<ListBox>();
//   auto& row = list.add<ListRow>("Default", icons::symbolic("emblem-system"));
//   row.set_detail("Active", glyph(Glyph::Check));
//   row.actions().add<Button>(glyph(Glyph::More), ButtonStyle::Flat);
//   list.selection_changed.connect([](ListRow& r) { ... });
//
// Rows show hover feedback (StateLayer), the theme's selection colours, and their trailing
// action buttons only while hovered, selected or focused. Up/Down move the selection.
#pragma once
#include <string>

#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/icon.h"
#include "uitk/widgets/label.h"

namespace uitk {

class ListRow : public Box {
public:
    explicit ListRow(std::string title, Image icon = {});

    void set_title(std::string t) { title_->set_text(std::move(t)); }
    void set_detail(std::string text, Image icon = {});   // dim secondary text after the title
    Box& actions() { return *actions_; }                   // trailing buttons (shown on hover)

    void set_selected(bool selected);
    bool selected() const { return state() & STATE_CHECKED; }

    Signal<> activated;   // click or Enter

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    void paint_over(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;

protected:
    void on_state_changed(uint32_t old_state) override;

private:
    friend class ListBox;
    void set_checked(bool on);
    void update_content();

    Icon* icon_;
    Label* title_;
    Icon* detail_icon_;
    Label* detail_;
    Box* actions_;
    StateLayer layer_;
};

class ListBox : public Box {
public:
    ListBox();

    void select(ListRow* row);           // nullptr clears the selection
    ListRow* selected() const { return selected_; }
    Signal<ListRow&> selection_changed;

    bool on_key(const KeyEvent& e) override;   // Up/Down/Home/End bubble here from the rows

private:
    ListRow* selected_ = nullptr;
};

} // namespace uitk
