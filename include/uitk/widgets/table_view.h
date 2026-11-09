// TableView: a virtualized data grid (query results, logs, process lists). Built on ListView.
//
//   struct Rows : TableModel {
//       size_t row_count() const override { return data.size(); }
//       std::string cell_text(size_t r, size_t c) const override { return data[r][c]; }
//       bool sort(size_t c, bool asc) override { ...reorder data...; return true; }
//   } model;
//   auto& table = box.add<TableView>();
//   table.set_columns({{"Name", 200}, {"Size", 90, 50, TextAlign::Right}});
//   table.set_model(&model);
//
// Columns: click a header to sort (again to reverse), drag a header edge to resize, double-click
// the edge to fit the content. Wide tables scroll horizontally (Shift+wheel, touchpad, the
// bottom scrollbar, Left/Right). Rows: everything ListView does (selection, keyboard, type-ahead).
#pragma once
#include <string>
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/list_view.h"

namespace uitk {

class ScrollBar;
class ScrollView;

struct TableColumn {
    std::string title;
    float width = 120;
    float min_width = 40;
    TextAlign align = TextAlign::Left;
    bool sortable = true;
    bool resizable = true;
};

class TableModel {
public:
    virtual ~TableModel() = default;
    virtual size_t row_count() const = 0;
    virtual std::string cell_text(size_t row, size_t column) const = 0;
    // Custom cell content (icons, colours, bars). Default: the text, aligned per column.
    virtual void paint_cell(PaintContext& ctx, size_t row, size_t column, const Rect& cell,
                            const RowState& state, const TableColumn& col);
    // Reorder the rows by a column. Return false when the column cannot be sorted.
    virtual bool sort(size_t /*column*/, bool /*ascending*/) { return false; }

    Signal<> changed;   // emit after rows changed
};

class TableView : public Widget {
public:
    TableView();
    ~TableView() override;

    void set_columns(std::vector<TableColumn> columns);
    const std::vector<TableColumn>& columns() const { return columns_; }
    void set_column_width(size_t column, float width);
    void autosize_column(size_t column);   // fit the header and the visible cells
    void set_model(TableModel* model);     // not owned
    TableModel* model() const { return model_; }

    void sort_by(size_t column, bool ascending);
    int sort_column() const { return sort_column_; }
    bool sort_ascending() const { return sort_ascending_; }

    void set_striped(bool on) { striped_ = on; invalidate(); }
    void set_grid_lines(bool on) { grid_ = on; invalidate(); }
    void set_stretch_last_column(bool on) { stretch_last_ = on; invalidate_layout(); }
    void set_selection_mode(SelectionMode m);

    float scroll_x() const { return scroll_x_; }
    void set_scroll_x(float x);
    float content_width() const;           // sum of column widths
    // Column x range in view coordinates (after horizontal scrolling and stretching).
    float column_x(size_t column) const;
    float column_display_width(size_t column) const;

    ListView& list() { return *list_; }
    Signal<size_t> activated;               // Enter / double-click on a row
    Signal<> selection_changed;
    Signal<size_t, bool> sorted;            // column, ascending (after the model sorted)

    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    bool on_scroll(const ScrollEvent& e) override;
    bool on_key(const KeyEvent& e) override;

    static constexpr float kCellPad = 8;

private:
    class Header;
    class Adapter;
    friend class Header;
    friend class Adapter;

    std::vector<TableColumn> columns_;
    TableModel* model_ = nullptr;
    ConnectionId model_conn_ = 0;
    std::unique_ptr<Adapter> adapter_;
    Header* header_;
    ScrollView* scroll_;
    ListView* list_;
    ScrollBar* hbar_;
    float scroll_x_ = 0;
    int sort_column_ = -1;
    bool sort_ascending_ = true;
    bool striped_ = false, grid_ = false, stretch_last_ = true;
};

} // namespace uitk
