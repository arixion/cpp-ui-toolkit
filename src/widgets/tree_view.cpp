#include "uitk/widgets/tree_view.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "uitk/widgets/containers.h"
#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/menu.h"

namespace uitk {

// ---------------------------------------------------------------------------- TreeNode

TreeNode::TreeNode(std::string t, Image i) : text(std::move(t)), icon(std::move(i)) {}

TreeNode& TreeNode::add(std::string t, Image i) {
    auto n = std::make_unique<TreeNode>(std::move(t), std::move(i));
    n->parent_ = this;
    children_.push_back(std::move(n));
    return *children_.back();
}

void TreeNode::remove(TreeNode* child) {
    std::erase_if(children_, [child](auto& c) { return c.get() == child; });
}

void TreeNode::clear() { children_.clear(); }

size_t TreeNode::depth() const {
    size_t d = 0;
    for (TreeNode* p = parent_; p && p->parent_; p = p->parent_) ++d;
    return d;
}

// ---------------------------------------------------------------------------- model

class TreeView::Model : public ListModel {
public:
    explicit Model(TreeView& v) : view_(v) {}
    size_t row_count() const override { return view_.rows_.size(); }
    std::string row_text(size_t i) const override { return view_.rows_[i]->text; }

    void paint_row(PaintContext& ctx, size_t i, const Rect& row, const RowState& st) override {
        const Theme& t = ctx.theme;
        TreeNode& n = *view_.rows_[i];
        size_t depth = n.depth();
        Color fg = st.selected && st.focused ? t.selection_fg : t.foreground;
        Color dim = st.selected && st.focused ? t.selection_fg_dim : t.foreground_dim;
        // Indent guides: one hairline per ancestor level.
        Color guide = t.border;
        for (size_t d = 0; d < depth; ++d) {
            float gx = std::round(view_.chevron_x(d) + 8);
            ctx.canvas.draw_rect({gx, row.y, 1, row.height}, Paint::fill(guide));
        }
        float cx = view_.chevron_x(depth);
        float cy = row.y + std::round((row.height - 16) / 2);
        if (n.has_children()) {
            Rect c{cx, cy, 16, 16};
            ctx.canvas.save();
            if (n.expanded_) ctx.canvas.rotate(90, c.center());
            ctx.canvas.draw_image(glyph(Glyph::ChevronRight), c, fg);
            ctx.canvas.restore();
        }
        float x = cx + 18;
        if (n.icon.valid()) {
            ctx.canvas.draw_image(n.icon, {x, cy, 16, 16}, dim);
            x += 22;
        }
        TextStyle s = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
        s.color = fg;
        float avail = std::max(0.f, row.right() - 8 - x);
        TextLayout tl(n.text, s, avail);
        ctx.canvas.draw_text(tl, {x, row.y + std::round((row.height - tl.size().height) / 2)});
        if (!n.detail.empty()) {
            float dx = x + tl.size().width + 8;
            if (dx < row.right() - 24) {
                TextStyle ds = s;
                ds.color = dim;
                ds.size = t.font_size_small;
                TextLayout dl(n.detail, ds, row.right() - 8 - dx);
                ctx.canvas.draw_text(dl, {dx, row.y + std::round((row.height - dl.size().height) / 2)});
            }
        }
    }

    bool row_pressed(size_t i, Point p, const PointerEvent& e) override {
        TreeNode& n = *view_.rows_[i];
        if (!n.has_children() || e.click_count != 1 || (e.mods & (mod::Ctrl | mod::Shift))) return false;
        float cx = view_.chevron_x(n.depth());
        bool on_chevron = p.x >= cx - 4 && p.x < cx + 20;
        // Toggling changes only the rows below this one, so the view can still select row i.
        if (on_chevron || view_.expand_on_click_) view_.toggle(n);
        return false;
    }

private:
    TreeView& view_;
};

// ---------------------------------------------------------------------------- TreeView

TreeView::TreeView() : model_(std::make_unique<Model>(*this)) {
    set_expand(true, true);
    scroll_ = &add<ScrollView>();
    list_ = &scroll_->set_content<ListView>();
    list_->set_model(model_.get());
    list_->activated.connect([this](size_t r) { activate_row(r); });
    list_->selection_changed.connect([this] { selection_changed.emit(); });
}

TreeView::~TreeView() { list_->set_model(nullptr); }

void TreeView::flatten(TreeNode& n, std::vector<TreeNode*>& out) {
    for (auto& c : n.children_) {
        out.push_back(c.get());
        if (c->expanded_) flatten(*c, out);
    }
}

void TreeView::rebuild() {
    // Keep the selection and the keyboard position on the same nodes.
    std::unordered_set<TreeNode*> sel;
    for (size_t r : list_->selection()) if (r < rows_.size()) sel.insert(rows_[r]);
    TreeNode* cur = current_node();
    rows_.clear();
    flatten(root_, rows_);
    model_->changed.emit();
    std::vector<size_t> rows;
    size_t cur_row = 0;
    for (size_t i = 0; i < rows_.size(); ++i) {
        if (sel.count(rows_[i])) rows.push_back(i);
        if (rows_[i] == cur) cur_row = i;
    }
    // A selected node hidden by collapsing passes the selection to its visible ancestor.
    if (cur && rows.empty() && !sel.empty()) {
        for (TreeNode* p = cur; p && p != &root_; p = p->parent_)
            if (long r = row_of(*p); r >= 0) { rows.push_back((size_t)r); cur_row = (size_t)r; break; }
    } else if (cur && row_of(*cur) < 0) {
        for (TreeNode* p = cur->parent_; p && p != &root_; p = p->parent_)
            if (long r = row_of(*p); r >= 0) { cur_row = (size_t)r; break; }
    }
    list_->set_selection(rows, cur_row);
}

void TreeView::refresh() { rebuild(); }

long TreeView::row_of(const TreeNode& node) const {
    auto it = std::find(rows_.begin(), rows_.end(), &node);
    return it == rows_.end() ? -1 : (long)(it - rows_.begin());
}

void TreeView::expand(TreeNode& n) {
    if (n.expanded_ || !n.has_children()) return;
    expanding.emit(n);   // lazy loading point
    n.expanded_ = true;
    rebuild();
    toggled.emit(n, true);
}

void TreeView::collapse(TreeNode& n) {
    if (!n.expanded_) return;
    n.expanded_ = false;
    rebuild();
    toggled.emit(n, false);
}

void TreeView::expand_all() {
    std::function<void(TreeNode&)> rec = [&](TreeNode& n) {
        for (auto& c : n.children_) {
            if (c->has_children() && !c->expanded_) { expanding.emit(*c); c->expanded_ = true; }
            rec(*c);
        }
    };
    rec(root_);
    rebuild();
}

void TreeView::collapse_all() {
    std::function<void(TreeNode&)> rec = [&](TreeNode& n) { for (auto& c : n.children_) { c->expanded_ = false; rec(*c); } };
    rec(root_);
    rebuild();
}

void TreeView::select(TreeNode& n) {
    bool changed_rows = false;
    for (TreeNode* p = n.parent_; p && p != &root_; p = p->parent_)
        if (!p->expanded_) { expanding.emit(*p); p->expanded_ = true; changed_rows = true; }
    if (changed_rows) rebuild();
    long r = row_of(n);
    if (r < 0) return;
    list_->select_only((size_t)r);
    list_->set_current((size_t)r);
}

std::vector<TreeNode*> TreeView::selected_nodes() const {
    std::vector<TreeNode*> out;
    for (size_t r : list_->selection()) if (r < rows_.size()) out.push_back(rows_[r]);
    return out;
}

TreeNode* TreeView::current_node() const {
    size_t c = list_->current();
    return c < rows_.size() ? rows_[c] : nullptr;
}

void TreeView::set_selection_mode(SelectionMode m) { list_->set_selection_mode(m); }
void TreeView::set_placeholder(std::string text) { list_->set_placeholder(std::move(text)); }

void TreeView::activate_row(size_t r) {
    if (r >= rows_.size()) return;
    TreeNode& n = *rows_[r];
    if (n.has_children()) {
        // A double-click's first press already toggled (expand on click): don't undo it.
        if (!expand_on_click_ || !(list_->state() & STATE_PRESSED)) toggle(n);
        return;
    }
    activated.emit(n);
}

bool TreeView::on_key(const KeyEvent& e) {
    if (!e.pressed || e.mods & (mod::Alt | mod::Ctrl)) return false;
    TreeNode* n = current_node();
    if (!n) return false;
    if (e.keysym == key::Right) {
        if (n->has_children() && !n->expanded_) expand(*n);
        else if (n->expanded_ && !n->children_.empty()) select(*n->children_.front());
        return true;
    }
    if (e.keysym == key::Left) {
        if (n->expanded_) collapse(*n);
        else if (n->parent_ && n->parent_ != &root_) select(*n->parent_);
        return true;
    }
    return false;
}

std::shared_ptr<Menu> TreeView::context_menu_at(Point) {
    if (menu_provider_) return menu_provider_(current_node());
    return Widget::context_menu_at({});
}

Size TreeView::measure(float available_width, const Theme& t) {
    return {std::max(160.f, available_width < 1e8f ? available_width : 0.f), 6 * t.list_row_height};
}

void TreeView::layout(const Theme& t) {
    scroll_->set_bounds(local_rect());
    scroll_->layout(t);
}

} // namespace uitk
