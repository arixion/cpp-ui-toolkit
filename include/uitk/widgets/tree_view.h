// TreeView: a virtualized, expandable tree (explorer, outline, settings tree). Built on
// ListView: the visible nodes are flattened into rows, so huge trees cost only their visible rows.
//
//   auto& tree = box.add<TreeView>();
//   TreeNode& src = tree.root().add("src", icons::for_file("src", true));
//   src.add("main.cpp", icons::for_file("main.cpp", false));
//   TreeNode& big = tree.root().add("node_modules");
//   big.set_lazy(true);                        // children are loaded on first expand:
//   tree.expanding.connect([](TreeNode& n) { if (n.lazy()) { ...add children...; n.set_lazy(false); } });
//   tree.refresh();                            // after changing nodes directly
//
// Keyboard: Up/Down/Home/End/PageUp/PageDown, Right expands (or goes to the first child), Left
// collapses (or goes to the parent), Enter/double-click activates leaves and toggles folders,
// type-ahead. Click on a folder row (or its chevron) toggles it.
#pragma once
#include <any>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/list_view.h"

namespace uitk {

class Menu;
class ScrollView;
class TreeView;

class TreeNode {
public:
    explicit TreeNode(std::string text = {}, Image icon = {});

    TreeNode& add(std::string text, Image icon = {});
    void remove(TreeNode* child);
    void clear();
    TreeNode* parent() const { return parent_; }
    const std::vector<std::unique_ptr<TreeNode>>& children() const { return children_; }
    size_t depth() const;   // top-level nodes: 0

    std::string text;
    Image icon;
    std::string detail;   // dim text after the label (description, status)
    std::any data;        // application data

    bool expanded() const { return expanded_; }
    // A lazy node shows a chevron before its children exist; they are added from `expanding`.
    void set_lazy(bool lazy) { lazy_ = lazy; }
    bool lazy() const { return lazy_; }
    bool has_children() const { return lazy_ || !children_.empty(); }

private:
    friend class TreeView;
    TreeNode* parent_ = nullptr;
    std::vector<std::unique_ptr<TreeNode>> children_;
    bool expanded_ = false;
    bool lazy_ = false;
};

class TreeView : public Widget {
public:
    TreeView();
    ~TreeView() override;

    TreeNode& root() { return root_; }        // invisible; its children are the top level
    void refresh();                           // rebuild rows after changing nodes directly
    void expand(TreeNode& node);
    void collapse(TreeNode& node);
    void toggle(TreeNode& node) { node.expanded_ ? collapse(node) : expand(node); }
    void expand_all();
    void collapse_all();

    void select(TreeNode& node);              // expands its ancestors and scrolls to it
    std::vector<TreeNode*> selected_nodes() const;
    TreeNode* current_node() const;
    TreeNode* node_at_row(size_t row) const { return row < rows_.size() ? rows_[row] : nullptr; }
    size_t row_count() const { return rows_.size(); }
    long row_of(const TreeNode& node) const;

    void set_selection_mode(SelectionMode m);
    void set_expand_on_click(bool on) { expand_on_click_ = on; }   // single click toggles folders
    void set_indent(float px) { indent_ = px; invalidate(); }
    void set_placeholder(std::string text);
    // Builds the context menu for a node (right-click selects it first; Menu key / Shift+F10).
    void set_context_menu_provider(std::function<std::shared_ptr<Menu>(TreeNode*)> fn) { menu_provider_ = std::move(fn); }

    ListView& list() { return *list_; }

    Signal<TreeNode&> activated;         // Enter / double-click on a leaf
    Signal<TreeNode&> expanding;         // before a node expands (load lazy children here)
    Signal<TreeNode&, bool> toggled;     // after expanding (true) / collapsing (false)
    Signal<> selection_changed;

    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    bool on_key(const KeyEvent& e) override;
    std::shared_ptr<Menu> context_menu_at(Point local) override;

private:
    class Model;
    friend class Model;
    void rebuild();                        // flatten + keep selection / cursor by node
    void flatten(TreeNode& n, std::vector<TreeNode*>& out);
    void activate_row(size_t row);
    float chevron_x(size_t depth) const { return 6 + depth * indent_; }

    TreeNode root_;
    std::vector<TreeNode*> rows_;
    std::unique_ptr<Model> model_;
    ScrollView* scroll_;
    ListView* list_;
    float indent_ = 12;
    bool expand_on_click_ = true;
    std::function<std::shared_ptr<Menu>(TreeNode*)> menu_provider_;
};

} // namespace uitk
