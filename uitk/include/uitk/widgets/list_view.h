// ListView: a virtualized list for large data sets (files, logs, query results).
//
// Rows are not widgets: a ListModel reports how many rows exist and paints a row's content.
// The view paints only the rows that are visible (put it in a ScrollView), so 100 000 rows cost
// the same as 20. It owns selection (single / multiple with Ctrl and Shift), the keyboard
// cursor, hover, row backgrounds, activation (double-click / Enter) and type-ahead search.
#pragma once
#include <chrono>
#include <string>
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/widget.h"

namespace uitk {

struct RowState {
    bool selected = false;
    bool hovered = false;
    bool cursor = false;    // keyboard position
    bool focused = false;   // the list has keyboard focus
};

class ListModel {
public:
    virtual ~ListModel() = default;
    virtual size_t row_count() const = 0;
    // Paint row content in `row` (list coordinates); the view already drew the background.
    virtual void paint_row(PaintContext& ctx, size_t index, const Rect& row, const RowState& state) = 0;
    // Text used for type-ahead search (optional).
    virtual std::string row_text(size_t) const { return {}; }
    // Rows that cannot be selected (e.g. files in a folder picker).
    virtual bool row_enabled(size_t) const { return true; }
    // A left press on a row (x/y in row coordinates), before the view selects it. Return true
    // to consume it (no selection change), e.g. for a button drawn inside the row.
    virtual bool row_pressed(size_t, Point /*in_row*/, const PointerEvent&) { return false; }

    Signal<> changed;   // emit after the rows changed (count or content)
};

enum class SelectionMode { None, Single, Multiple };

class ListView : public Widget {
public:
    ListView();
    ~ListView() override;

    void set_model(ListModel* model);   // not owned
    ListModel* model() const { return model_; }
    void set_row_height(float h) { row_height_ = h; invalidate_layout(); }   // 0 = theme
    float row_height() const;
    void set_selection_mode(SelectionMode m);
    // Text shown when there are no rows ("This folder is empty"). The list stays visible and
    // focusable, so keyboard navigation keeps working.
    void set_placeholder(std::string text) { if (text != placeholder_) { placeholder_ = std::move(text); invalidate(); } }

    std::vector<size_t> selection() const;   // ascending
    bool is_selected(size_t i) const { return i < selected_.size() && selected_[i]; }
    void select_only(size_t i);
    void clear_selection();
    void select_all();
    size_t current() const { return cursor_; }   // keyboard position
    void set_current(size_t i, bool scroll = true);
    // Replace the selection (e.g. restoring it after the model changed). No signal.
    void set_selection(const std::vector<size_t>& rows, size_t current);
    Rect row_rect(size_t i) const { return {0, i * row_height(), size().width, row_height()}; }

    Signal<> selection_changed;
    Signal<size_t> activated;   // double-click / Enter on a row

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;
    bool on_text(const TextEvent& e) override;
    Rect keyboard_anchor() const override { return count() ? row_rect(cursor_) : local_rect(); }

protected:
    void on_state_changed(uint32_t old_state) override;

private:
    void model_changed();
    long row_at(float y) const;
    void click_select(size_t i, uint32_t mods);
    void move_cursor(long to, uint32_t mods);
    void select_range(size_t a, size_t b, bool additive);
    size_t count() const { return model_ ? model_->row_count() : 0; }

    ListModel* model_ = nullptr;
    ConnectionId model_conn_ = 0;
    float row_height_ = 0;
    SelectionMode mode_ = SelectionMode::Single;
    std::vector<bool> selected_;
    size_t cursor_ = 0, anchor_ = 0;
    long hover_ = -1;
    std::string typeahead_;
    std::string placeholder_;
    std::chrono::steady_clock::time_point typeahead_time_{};
};

} // namespace uitk
