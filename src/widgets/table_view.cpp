#include "uitk/widgets/table_view.h"

#include <algorithm>
#include <cmath>

#include "uitk/widgets/containers.h"
#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/label.h"

namespace uitk {

namespace {
constexpr float kEdgeGrab = 4;
}

void TableModel::paint_cell(PaintContext& ctx, size_t row, size_t column, const Rect& cell,
                            const RowState& st, const TableColumn& col) {
    const Theme& t = ctx.theme;
    TextStyle s = Label::style_for(t, TextRole::Body, col.align, true);
    s.color = st.selected && st.focused ? t.selection_fg : t.foreground;
    float pad = TableView::kCellPad;
    TextLayout tl(cell_text(row, column), s, std::max(0.f, cell.width - 2 * pad));
    ctx.canvas.draw_text(tl, {cell.x + pad, cell.y + std::round((cell.height - tl.size().height) / 2)});
}

// ---------------------------------------------------------------------------- rows

class TableView::Adapter : public ListModel {
public:
    explicit Adapter(TableView& v) : view_(v) {}
    size_t row_count() const override { return view_.model_ ? view_.model_->row_count() : 0; }
    std::string row_text(size_t i) const override {
        return view_.model_ && !view_.columns_.empty() ? view_.model_->cell_text(i, 0) : std::string();
    }
    void paint_row(PaintContext& ctx, size_t i, const Rect& row, const RowState& st) override {
        const Theme& t = ctx.theme;
        if (view_.striped_ && (i % 2) && !st.selected && !st.hovered)
            ctx.canvas.draw_rect(row, Paint::fill(t.button_flat.overlay.with_opacity(0.035f)));
        for (size_t c = 0; c < view_.columns_.size(); ++c) {
            float x = view_.column_x(c), w = view_.column_display_width(c);
            if (x >= row.right() || x + w <= row.x) continue;
            Rect cell{x, row.y, w, row.height};
            ctx.canvas.save();
            ctx.canvas.clip_rect(cell);
            view_.model_->paint_cell(ctx, i, c, cell, st, view_.columns_[c]);
            ctx.canvas.restore();
            if (view_.grid_) ctx.canvas.draw_rect({std::round(x + w) - 1, row.y, 1, row.height}, Paint::fill(t.border));
        }
        if (view_.grid_) ctx.canvas.draw_rect({row.x, row.bottom() - 1, row.width, 1}, Paint::fill(t.border));
    }

private:
    TableView& view_;
};

// ---------------------------------------------------------------------------- header

class TableView::Header : public Widget {
public:
    explicit Header(TableView& v) : view_(v) {}
    bool accepts_pointer() const override { return true; }
    MouseCursor cursor() const override { return (edge_ >= 0 || drag_col_ >= 0) ? MouseCursor::ResizeE : MouseCursor::Default; }
    Size measure(float, const Theme& t) override { return {0, t.list_row_height + 2}; }

    // Index of the column whose right edge is under x (resizable columns only).
    int edge_at(float x) const {
        for (size_t c = 0; c < view_.columns_.size(); ++c) {
            float e = view_.column_x(c) + view_.column_display_width(c);
            if (std::abs(x - e) <= kEdgeGrab && view_.columns_[c].resizable) return (int)c;
        }
        return -1;
    }
    int column_at(float x) const {
        for (size_t c = 0; c < view_.columns_.size(); ++c) {
            float cx = view_.column_x(c);
            if (x >= cx && x < cx + view_.column_display_width(c)) return (int)c;
        }
        return -1;
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ctx.theme;
        Rect r = local_rect();
        ctx.canvas.save();
        ctx.canvas.clip_rect(r);
        TextStyle base = Label::style_for(t, TextRole::Chrome, TextAlign::Left, true);
        base.weight = 600;
        for (size_t c = 0; c < view_.columns_.size(); ++c) {
            const TableColumn& col = view_.columns_[c];
            float x = view_.column_x(c), w = view_.column_display_width(c);
            if (x >= r.width || x + w <= 0) continue;
            Rect cell{x, 0, w, r.height};
            if ((int)c == hover_col_ && col.sortable && edge_ < 0 && drag_col_ < 0)
                ctx.canvas.draw_rect(cell, Paint::fill(t.button_flat.overlay.with_opacity(t.hover_opacity * 0.6f)));
            bool sorted = (int)c == view_.sort_column_;
            float indicator = sorted ? 18.f : 0.f;
            TextStyle s = base;
            s.align = col.align;
            s.color = sorted ? t.foreground : t.chrome_fg;
            float pad = kCellPad;
            float tw = std::max(0.f, w - 2 * pad - indicator);
            TextLayout tl(col.title, s, tw);
            float tx = cell.x + pad + (col.align == TextAlign::Right ? indicator : 0);
            ctx.canvas.draw_text(tl, {tx, std::round((r.height - tl.size().height) / 2)});
            if (sorted) {
                float gx = col.align == TextAlign::Right ? cell.x + pad - 2 : cell.right() - pad - 16 + 2;
                ctx.canvas.draw_image(glyph(view_.sort_ascending_ ? Glyph::ChevronUp : Glyph::ChevronDown),
                                      {gx, std::round((r.height - 16) / 2), 16, 16}, t.foreground);
            }
            // Column separator (stronger while it is the resize target).
            bool active = (int)c == edge_ || (int)c == drag_col_;
            Color sep = active ? t.focus_ring : t.border;
            float ex = std::round(x + w) - 1;
            if (active) ctx.canvas.draw_rect({ex - 1, 0, 3, r.height}, Paint::fill(sep));
            else ctx.canvas.draw_rect({ex, 5, 1, r.height - 10}, Paint::fill(sep));
        }
        ctx.canvas.restore();
        ctx.canvas.draw_rect({0, r.height - 1, r.width, 1}, Paint::fill(t.border));
    }

    bool on_pointer(const PointerEvent& e) override {
        switch (e.type) {
            case PointerEvent::Type::Enter:
            case PointerEvent::Type::Move: {
                if (drag_col_ >= 0) {
                    view_.set_column_width((size_t)drag_col_, drag_width_ + (e.pos.x - drag_x_));
                    return true;
                }
                int edge = edge_at(e.pos.x), col = column_at(e.pos.x);
                if (edge != edge_ || col != hover_col_) { edge_ = edge; hover_col_ = col; invalidate(); }
                return true;
            }
            case PointerEvent::Type::Down: {
                if (e.button != PointerButton::Left) return true;
                int edge = edge_at(e.pos.x);
                if (edge >= 0) {
                    if (e.click_count >= 2) { view_.autosize_column((size_t)edge); return true; }
                    drag_col_ = edge;
                    drag_x_ = e.pos.x;
                    drag_width_ = view_.columns_[(size_t)edge].width;
                    return true;
                }
                press_col_ = column_at(e.pos.x);
                return true;
            }
            case PointerEvent::Type::Up: {
                if (drag_col_ >= 0) { drag_col_ = -1; invalidate(); return true; }
                int col = column_at(e.pos.x);
                if (col >= 0 && col == press_col_ && view_.columns_[(size_t)col].sortable) {
                    bool asc = view_.sort_column_ == col ? !view_.sort_ascending_ : true;
                    view_.sort_by((size_t)col, asc);
                }
                press_col_ = -1;
                return true;
            }
            default: return true;
        }
    }

protected:
    void on_state_changed(uint32_t) override {
        if (!(state() & STATE_HOVERED) && drag_col_ < 0 && (edge_ >= 0 || hover_col_ >= 0)) {
            edge_ = hover_col_ = -1;
            invalidate();
        }
    }

private:
    TableView& view_;
    int edge_ = -1, hover_col_ = -1, press_col_ = -1, drag_col_ = -1;
    float drag_x_ = 0, drag_width_ = 0;
};

// ---------------------------------------------------------------------------- TableView

TableView::TableView() : adapter_(std::make_unique<Adapter>(*this)) {
    set_expand(true, true);
    set_clip_children(true);
    header_ = &add<Header>(*this);
    scroll_ = &add<ScrollView>();
    list_ = &scroll_->set_content<ListView>();
    list_->set_model(adapter_.get());
    list_->activated.connect([this](size_t r) { activated.emit(r); });
    list_->selection_changed.connect([this] { selection_changed.emit(); });
    hbar_ = &add<ScrollBar>(Orientation::Horizontal);
    hbar_->scrolled.connect([this](float x) { set_scroll_x(x); });
}

TableView::~TableView() {
    if (model_) model_->changed.disconnect(model_conn_);
    list_->set_model(nullptr);
}

void TableView::set_columns(std::vector<TableColumn> columns) {
    columns_ = std::move(columns);
    sort_column_ = -1;
    invalidate_layout();
}

void TableView::set_column_width(size_t c, float w) {
    if (c >= columns_.size()) return;
    w = std::round(std::max(w, columns_[c].min_width));
    if (w == columns_[c].width) return;
    columns_[c].width = w;
    invalidate_layout();
}

void TableView::set_model(TableModel* m) {
    if (model_) model_->changed.disconnect(model_conn_);
    model_ = m;
    if (model_) model_conn_ = model_->changed.connect([this] { adapter_->changed.emit(); });
    sort_column_ = -1;
    adapter_->changed.emit();
}

void TableView::set_selection_mode(SelectionMode m) { list_->set_selection_mode(m); }

void TableView::sort_by(size_t c, bool asc) {
    if (!model_ || c >= columns_.size() || !columns_[c].sortable) return;
    if (!model_->sort(c, asc)) return;
    sort_column_ = (int)c;
    sort_ascending_ = asc;
    adapter_->changed.emit();
    header_->invalidate();
    sorted.emit(c, asc);
}

float TableView::content_width() const {
    float w = 0;
    for (auto& c : columns_) w += c.width;
    return w;
}

float TableView::column_x(size_t c) const {
    float x = -scroll_x_;
    for (size_t i = 0; i < c && i < columns_.size(); ++i) x += columns_[i].width;
    return x;
}

float TableView::column_display_width(size_t c) const {
    if (c >= columns_.size()) return 0;
    float w = columns_[c].width;
    // The last column takes the rest of the view (no empty strip on the right).
    if (stretch_last_ && c + 1 == columns_.size()) w = std::max(w, size().width - (content_width() - w));
    return w;
}

void TableView::autosize_column(size_t c) {
    if (c >= columns_.size()) return;
    const Theme& t = theme();
    TextStyle hs = Label::style_for(t, TextRole::Chrome, TextAlign::Left, true);
    hs.weight = 600;
    float w = TextLayout(columns_[c].title, hs).size().width + 18;
    if (model_) {
        TextStyle s = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
        Rect vis = list_->visible_rect();
        float rh = list_->row_height();
        size_t n = model_->row_count();
        size_t first = (size_t)std::max(0.f, std::floor(vis.y / rh));
        size_t last = std::min(n, (size_t)std::ceil(vis.bottom() / rh) + 1);
        for (size_t r = first; r < last; ++r) w = std::max(w, TextLayout(model_->cell_text(r, c), s).size().width);
    }
    set_column_width(c, std::ceil(w) + 2 * kCellPad + 2);
}

void TableView::set_scroll_x(float x) {
    float max_x = std::max(0.f, content_width() - size().width);
    x = std::round(std::clamp(x, 0.f, max_x));
    if (x == scroll_x_) return;
    scroll_x_ = x;
    hbar_->set_metrics(content_width(), size().width, scroll_x_);
    hbar_->flash();
    list_->invalidate();
    header_->invalidate();
}

Size TableView::measure(float available_width, const Theme& t) {
    return {std::max(200.f, available_width < 1e8f ? available_width : 0.f), t.list_row_height * 7 + 2};
}

void TableView::layout(const Theme& t) {
    float w = size().width, h = size().height;
    float hh = header_->measure_clamped(w, t).height;
    header_->set_bounds({0, 0, w, hh});
    scroll_->set_bounds({0, hh, w, std::max(0.f, h - hh)});
    scroll_->layout(t);
    float sw = t.scrollbar_width;
    hbar_->set_bounds({0, h - sw, w, sw});
    scroll_x_ = std::clamp(scroll_x_, 0.f, std::max(0.f, content_width() - w));
    hbar_->set_metrics(content_width(), w, scroll_x_);
}

bool TableView::on_scroll(const ScrollEvent& e) {
    float dx = e.dx != 0 ? e.dx : ((e.mods & MOD_SHIFT) ? e.dy : 0.f);
    if (dx == 0 || content_width() <= size().width) return false;
    set_scroll_x(scroll_x_ + dx * 3);
    return true;
}

bool TableView::on_key(const KeyEvent& e) {
    if (!e.pressed || (e.mods & (MOD_ALT | MOD_CTRL))) return false;
    if (e.keysym == key::Left) { set_scroll_x(scroll_x_ - 40); return true; }
    if (e.keysym == key::Right) { set_scroll_x(scroll_x_ + 40); return true; }
    return false;
}

} // namespace uitk
