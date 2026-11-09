// Application-structure controls: menus, context menus, menu bar, dialogs, splitter, expander,
// search field, tree view and table view. Real events through an offscreen WindowShell.
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "uitk/widgets/dialog.h"
#include "uitk/widgets/expander.h"
#include "uitk/widgets/menu.h"
#include "uitk/widgets/paned.h"
#include "uitk/widgets/search_field.h"
#include "uitk/widgets/table_view.h"
#include "uitk/render/renderer.h"
#include "uitk/widgets/text_field.h"
#include "uitk/widgets/toggles.h"
#include "uitk/widgets/tree_view.h"
#include "uitk/widgets/window_shell.h"

using namespace uitk;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

namespace {
WindowShell* g_shell = nullptr;
const Size kSize{1000, 700};
void relayout() { g_shell->host().layout(kSize); }
void send(const Event& e) { g_shell->handle_event(e); relayout(); }
PointerEvent ptr(PointerEvent::Type t, Point p, PointerButton b = PointerButton::Left, int clicks = 1, uint32_t mods = 0) {
    PointerEvent e; e.type = t; e.pos = p; e.button = b; e.click_count = clicks; e.mods = mods;
    return e;
}
Point center(Widget& w) { return w.to_window(w.local_rect().center()); }
void click_at(Point p, PointerButton b = PointerButton::Left, int clicks = 1, uint32_t mods = 0) {
    send(ptr(PointerEvent::Type::Move, p));
    send(ptr(PointerEvent::Type::Down, p, b, clicks, mods));
    send(ptr(PointerEvent::Type::Up, p, b, clicks, mods));
}
void click(Widget& w, int clicks = 1) { click_at(center(w), PointerButton::Left, clicks); }
void press(uint32_t sym, uint32_t mods = 0) {
    KeyEvent k; k.keysym = sym; k.mods = mods; k.pressed = true; send(k);
    k.pressed = false; send(k);
}
void type(const char* s) { send(TextEvent{s}); }
MenuPopup* top_menu() { return dynamic_cast<MenuPopup*>(g_shell->host().top_popup()); }
} // namespace

static void test_shortcuts() {
    Shortcut s = Shortcut::parse("Ctrl+Shift+P");
    CHECK(s.keysym == 'p' && s.mods == (mod::Ctrl | mod::Shift));
    CHECK(s.text() == "Ctrl+Shift+P");
    CHECK(Shortcut::parse("F5").keysym == key::F1 + 4 && Shortcut::parse("F5").text() == "F5");
    CHECK(Shortcut::parse("Alt+Up").text() == "Alt+Up");
    CHECK(Shortcut::parse("Ctrl++").keysym == '+');
    CHECK(Shortcut::parse("Ctrl+,").text() == "Ctrl+,");
    CHECK(!Shortcut::parse("Hyper+X"));
    KeyEvent k; k.keysym = 'P'; k.mods = mod::Ctrl | mod::Shift;   // Shift gives the upper-case keysym
    CHECK(s.matches(k));
    k.mods = mod::Ctrl;
    CHECK(!s.matches(k));
    MenuItem it; it.label = "Save &As && Close";
    CHECK(it.display_label() == "Save As & Close" && it.mnemonic() == 'a' && it.mnemonic_offset() == 5);
}

static void test_menus() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    Label& target = shell.content().add<Label>("right-click me");
    target.set_min_size({300, 100});
    relayout();

    int news = 0, opens = 0;
    bool wrap = false;
    int theme = 0;
    auto menu = std::make_shared<Menu>();
    menu->add("&New", [&] { ++news; }, Shortcut::parse("Ctrl+N"));
    menu->add("Disabled", [&] { news += 100; }).set_enabled(false);
    menu->add_separator();
    menu->add_check("&Word Wrap", false, [&](bool on) { wrap = on; });
    menu->add_radio(1, "&Light", true, [&] { theme = 0; });
    menu->add_radio(1, "&Dark", false, [&] { theme = 1; });
    Menu& recent = menu->add_submenu("Open &Recent");
    recent.add("project-a", [&] { ++opens; });
    recent.add("project-b", [&] { opens += 10; });
    target.set_context_menu(menu);

    // Right-click opens the context menu at the pointer; releasing the button there does nothing.
    Point p = center(target);
    click_at(p, PointerButton::Right);
    MenuPopup* m = top_menu();
    CHECK(m && m->bounds().x == p.x && m->bounds().y == p.y);
    CHECK(news == 0);
    // Keyboard: Down skips the disabled item and the separator.
    press(key::Down);
    CHECK(m->highlighted() == 0);
    press(key::Down);
    CHECK(m->highlighted() == 3);
    press(key::Return);   // toggles Word Wrap and closes
    CHECK(wrap && !shell.host().has_popup());
    CHECK(menu->items()[3].checked);

    // Radio items are exclusive; mnemonics activate.
    popup_menu(shell.host(), menu, Point{50, 50});
    type("d");
    CHECK(theme == 1 && menu->items()[5].checked && !menu->items()[4].checked && !shell.host().has_popup());

    // Submenu by keyboard: Right opens it (focused, first item highlighted), Left closes it.
    m = &popup_menu(shell.host(), menu, Point{50, 50}, true);
    CHECK(m->highlighted() == 0);
    press(key::End);
    CHECK(m->highlighted() == 6);
    press(key::Right);
    MenuPopup* sub = top_menu();
    CHECK(sub && sub != m && sub->parent_menu() == m && sub->highlighted() == 0);
    CHECK(sub->bounds().x >= m->bounds().right());
    press(key::Left);
    CHECK(top_menu() == m && !m->open_submenu());
    press(key::Right);
    press(key::Down);
    press(key::Return);   // project-b: closes the whole chain
    CHECK(opens == 10 && !shell.host().has_popup());

    // Hover opens the submenu (no application: immediately); a click on the parent menu closes
    // only the submenu and reaches the parent; a click outside closes everything.
    m = &popup_menu(shell.host(), menu, Point{50, 50});
    relayout();
    Point sub_row = m->to_window(m->item_rect(6).center());   // "Open Recent"
    send(ptr(PointerEvent::Type::Move, sub_row));
    CHECK(m->highlighted() == 6 && m->open_submenu());
    sub = m->open_submenu();
    if (!sub) return;
    send(ptr(PointerEvent::Type::Move, center(*sub)));
    CHECK(m->open_submenu() == sub);   // moving into the submenu keeps it
    Point first_row = m->to_window(m->item_rect(0).center());
    send(ptr(PointerEvent::Type::Move, first_row));   // hovering another parent item closes it
    CHECK(!m->open_submenu() && m->highlighted() == 0);
    send(ptr(PointerEvent::Type::Move, sub_row));
    CHECK(m->open_submenu());
    click_at({900, 600});   // outside: whole chain closes, click consumed
    CHECK(!shell.host().has_popup());

    // Escape closes only the top menu of a chain.
    m = &popup_menu(shell.host(), menu, Point{50, 50}, true);
    press(key::End);
    press(key::Right);
    press(key::Escape);
    CHECK(top_menu() == m);
    press(key::Escape);
    CHECK(!shell.host().has_popup());

    // Menu key opens the focused widget's context menu.
    TextField& field = shell.content().add<TextField>();
    field.set_context_menu(menu);
    relayout();
    field.grab_focus();
    press(0xff67);
    CHECK(top_menu() != nullptr);
    press(key::Escape);
}

static void test_menu_bar() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    MenuBar& bar = shell.header_bar().start().add<MenuBar>();
    int saved = 0, undone = 0;
    Menu& file = bar.add_menu("&File");
    file.add("&Save", [&] { ++saved; }, Shortcut::parse("Ctrl+S"));
    Menu& edit = bar.add_menu("&Edit");
    edit.add("&Undo", [&] { ++undone; }, Shortcut::parse("Ctrl+Z"));
    bar.add_menu("&View").add("Zoom");
    TextField& field = shell.content().add<TextField>();
    relayout();
    CHECK(bar.size().width > 60 && bar.size().height > 10);

    // Shortcuts work application-wide (the focused text field doesn't use Ctrl+S).
    field.grab_focus();
    press('s', mod::Ctrl);
    CHECK(saved == 1);
    // Alt+mnemonic opens a menu from the keyboard; Right/Left switch menus.
    press('e', mod::Alt);
    CHECK(bar.open_index() == 1 && top_menu() && top_menu()->highlighted() == 0);
    press(key::Right);
    CHECK(bar.open_index() == 2);
    press(key::Right);
    CHECK(bar.open_index() == 0);   // wraps
    press(key::Left);
    press(key::Left);
    CHECK(bar.open_index() == 1);
    press(key::Return);
    CHECK(undone == 1 && bar.open_index() == -1 && !shell.host().has_popup());
    CHECK(field.has_focus());   // focus returned
    // Click opens; hovering another title while open switches.
    Point file_title = bar.to_window({10, bar.size().height / 2});
    click_at(file_title);
    CHECK(bar.open_index() == 0);
    send(ptr(PointerEvent::Type::Move, bar.to_window({bar.size().width - 10, bar.size().height / 2})));
    CHECK(bar.open_index() == 2);
    click_at(bar.to_window({bar.size().width - 10, bar.size().height / 2}));   // same title: closes
    CHECK(bar.open_index() == -1 && !shell.host().has_popup());
    // F10 opens the first menu; no shortcuts while a modal dialog is open.
    press(0xffc7);
    CHECK(bar.open_index() == 0);
    press(key::Escape);
    Dialog::message(shell.host(), MessageKind::Info, "Hi", "");
    press('s', mod::Ctrl);
    CHECK(saved == 1);
    press(key::Escape);
    press('s', mod::Ctrl);
    CHECK(saved == 2);
}

static void test_dialogs() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    Button& under = shell.content().add<Button>("under");
    relayout();
    int under_clicks = 0;
    under.clicked.connect([&] { ++under_clicks; });

    int got = 99;
    Dialog& d = Dialog::message(shell.host(), MessageKind::Warning, "Delete 3 files?", "This cannot be undone.",
                                {{"Cancel", Dialog::Cancel}, {"Delete", Dialog::Ok, ButtonStyle::Destructive}},
                                [&](int r) { got = r; });
    relayout();
    CHECK(d.bounds().width >= 360 && d.bounds().width <= 560);
    CHECK(std::abs(d.bounds().center().x - kSize.width / 2) < 1);
    CHECK(shell.host().focus() == d.button_for(Dialog::Ok));   // default button focused
    click(under);
    CHECK(under_clicks == 0 && shell.host().has_popup());      // modal
    press(key::Escape);
    CHECK(got == Dialog::Cancel && !shell.host().has_popup());

    Dialog::message(shell.host(), MessageKind::Question, "Save?", "", {{"No", Dialog::No}, {"Yes", Dialog::Yes, ButtonStyle::Primary}},
                    [&](int r) { got = r; });
    press(key::Return);
    CHECK(got == Dialog::Yes);

    // Custom dialog: a text field in the body keeps focus; Enter (unused by the field) responds.
    Dialog& rename = Dialog::show<Dialog>(shell.host(), "Rename");
    TextField& name = rename.body().add<TextField>("old.txt");
    rename.add_button("Cancel", Dialog::Cancel);
    rename.add_button("Rename", Dialog::Ok, ButtonStyle::Primary);
    rename.set_default_response(Dialog::Ok);
    std::string result;
    rename.responded.connect([&](int r) { if (r == Dialog::Ok) result = name.text(); });
    shell.host().close_popup(&rename);   // closing without a response = cancel
    CHECK(result.empty());

    Dialog& again = Dialog::show<Dialog>(shell.host(), "Rename");
    TextField& f2 = again.body().add<TextField>("a");
    again.add_button("OK", Dialog::Ok, ButtonStyle::Primary);
    again.set_default_response(Dialog::Ok);
    int r2 = 0;
    again.responded.connect([&](int r) { r2 = r; });
    relayout();
    f2.grab_focus();
    type("b");
    CHECK(f2.text() == "ab");
    click(*again.button_for(Dialog::Ok));
    CHECK(r2 == Dialog::Ok && !shell.host().has_popup());
    // A dialog opened from a response handler (after the first closed) works.
    bool second = false;
    Dialog::message(shell.host(), MessageKind::None, "One", "", {{"OK", Dialog::Ok}}, [&](int) {
        Dialog::message(shell.host(), MessageKind::None, "Two", "", {{"OK", Dialog::Ok}}, [&](int) { second = true; });
    });
    press(key::Return);
    CHECK(shell.host().has_popup());
    press(key::Return);
    CHECK(second && !shell.host().has_popup());
}

static void test_paned() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    Paned& p = shell.content().add<Paned>(Orientation::Horizontal);
    Widget& a = p.set_start<Widget>();
    Widget& b = p.set_end<Widget>();
    p.set_position(250);
    p.set_min_sizes(100, 200);
    relayout();
    CHECK(a.size().width == 250 && b.bounds().x == 251);
    float pw = p.size().width;
    // Drag the sash.
    Point sash = p.to_window({250, 100});
    send(ptr(PointerEvent::Type::Move, sash));
    CHECK(shell.host().cursor() == MouseCursor::ResizeE);
    send(ptr(PointerEvent::Type::Down, sash));
    send(ptr(PointerEvent::Type::Move, {sash.x + 60, sash.y}));
    send(ptr(PointerEvent::Type::Up, {sash.x + 60, sash.y}));
    CHECK(p.position() == 310 && a.size().width == 310);
    // Clamped by the minimum sizes.
    send(ptr(PointerEvent::Type::Down, {sash.x + 60, sash.y}));
    send(ptr(PointerEvent::Type::Move, {pw + 500, sash.y}));
    send(ptr(PointerEvent::Type::Up, {pw + 500, sash.y}));
    CHECK(p.position() == pw - 1 - 200);
    // Double-click restores the initial position.
    Point s2 = p.to_window({p.position(), 100});
    click_at(s2, PointerButton::Left, 2);
    CHECK(p.position() == 250);
    // Keyboard (focused sash).
    shell.host().focus_next(false);
    press(key::Right);
    CHECK(p.position() == 260);
    press(key::Left, mod::Shift);
    CHECK(p.position() == 210);
    // KeepEnd: the end pane keeps its size when the window grows.
    p.set_resize_policy(PanedResize::KeepEnd);
    float end_w = b.size().width;
    shell.host().layout({1200, 700});
    CHECK(std::abs(b.size().width - end_w) < 1);
}

static void test_expander() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    Expander& ex = shell.content().add<Expander>("Advanced", false);
    CheckBox& cb = ex.content().add<CheckBox>("Verbose");
    Button& after = shell.content().add<Button>("after");
    relayout();
    float collapsed_h = ex.size().height;
    CHECK(!cb.visible() || cb.size().height == 0 || ex.size().height < 40);
    int toggles = 0;
    ex.toggled.connect([&](bool) { ++toggles; });
    click_at(ex.to_window({30, collapsed_h / 2}));
    CHECK(ex.expanded() && toggles == 1);
    relayout();
    CHECK(ex.size().height > collapsed_h + 15 && after.to_window({0, 0}).y >= cb.to_window({0, cb.size().height}).y);
    // Keyboard on the focused header: Left collapses, Right expands, Space toggles.
    press(key::Left);
    CHECK(!ex.expanded());
    // Offscreen frames are settled: one paint finishes the collapse (content leaves the Tab chain).
    auto r = OffscreenRenderer::create({1000, 700}, 1.f);
    shell.paint(*r->begin_frame(), kSize);
    r->end_frame();
    relayout();
    CHECK(!ex.content().visible() && std::abs(ex.size().height - collapsed_h) < 0.5f);
    press(key::Right);
    CHECK(ex.expanded() && ex.content().visible());
    press(0x20);
    CHECK(!ex.expanded() && toggles == 4);

    Accordion& acc = shell.content().add<Accordion>();
    Expander& s1 = acc.add_section("Open Editors", true);
    Expander& s2 = acc.add_section("Outline");
    acc.set_exclusive(true);
    s2.set_expanded(true);
    CHECK(s2.expanded() && !s1.expanded());
}

static void test_search_field() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    SearchField& s = shell.content().add<SearchField>("Find");
    ToggleButton& opt = s.add_option(Image(), "Match Case");
    s.set_history_enabled(true);
    relayout();
    std::vector<std::string> seen;
    s.search_changed.connect([&](const std::string& q) { seen.push_back(q); });
    std::string act;
    s.activated.connect([&](const std::string& q) { act = q; });
    click(s.field());
    type("ab");
    CHECK(seen.size() == 1 && seen.back() == "ab");   // no app: no debounce
    press(key::Return);
    CHECK(act == "ab" && s.history().size() == 1);
    press(key::Escape);
    CHECK(s.text().empty() && seen.back().empty());
    press(key::Up);
    CHECK(s.text() == "ab");
    press(key::Down);
    CHECK(s.text().empty());
    s.set_text("x");
    CHECK(seen.back() == "x");
    size_t n = seen.size();
    s.set_text("x");   // unchanged: no emission
    CHECK(seen.size() == n);
    click(opt);
    CHECK(opt.checked());
}

static void test_tree() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    TreeView& tree = shell.content().add<TreeView>();
    TreeNode& src = tree.root().add("src");
    TreeNode& widgets = src.add("widgets");
    widgets.add("button.cpp");
    widgets.add("menu.cpp");
    src.add("main.cpp");
    TreeNode& lazy = tree.root().add("deps");
    lazy.set_lazy(true);
    tree.root().add("README.md");
    int loads = 0;
    tree.expanding.connect([&](TreeNode& n) {
        if (&n == &lazy && n.lazy()) { ++loads; n.add("libfoo"); n.add("libbar"); n.set_lazy(false); }
    });
    std::string activated;
    tree.activated.connect([&](TreeNode& n) { activated = n.text; });
    tree.refresh();
    relayout();
    CHECK(tree.row_count() == 3);
    CHECK(widgets.depth() == 1 && src.depth() == 0);

    // Click a folder: selects and expands it.
    ListView& list = tree.list();
    float rh = list.row_height();
    click_at(list.to_window({60, rh * 0.5f}));
    CHECK(src.expanded() && tree.row_count() == 5 && tree.current_node() == &src);
    // Keyboard: Down to "widgets", Right expands, Right again goes to its first child.
    press(key::Down);
    CHECK(tree.current_node() == &widgets);
    press(key::Right);
    CHECK(widgets.expanded() && tree.row_count() == 7);
    press(key::Right);
    CHECK(tree.current_node()->text == "button.cpp");
    press(key::Return);
    CHECK(activated == "button.cpp");
    // Left on a leaf goes to the parent; Left again collapses it; the selection follows the node.
    press(key::Left);
    CHECK(tree.current_node() == &widgets);
    press(key::Left);
    CHECK(!widgets.expanded() && tree.row_count() == 5);
    CHECK(tree.selected_nodes().size() == 1 && tree.selected_nodes()[0] == &widgets);
    // Lazy children load on first expand only.
    tree.select(lazy);
    press(key::Right);
    CHECK(loads == 1 && lazy.expanded() && tree.row_count() == 7);
    tree.collapse(lazy);
    tree.expand(lazy);
    CHECK(loads == 1);
    // Collapsing a parent of the selection moves the selection to the parent.
    tree.select(*widgets.children()[1]);
    CHECK(widgets.expanded() && tree.current_node()->text == "menu.cpp");
    tree.collapse(src);
    CHECK(tree.current_node() == &src && tree.selected_nodes()[0] == &src);
    // Context menu provider gets the node under a right-click.
    TreeNode* menu_for = nullptr;
    tree.set_context_menu_provider([&](TreeNode* n) {
        menu_for = n;
        auto m = std::make_shared<Menu>();
        m->add("Rename");
        return m;
    });
    tree.collapse_all();
    click_at(list.to_window({60, rh * 2.5f}), PointerButton::Right);
    CHECK(menu_for && menu_for->text == "README.md" && shell.host().has_popup());
    press(key::Escape);
    // Type-ahead.
    list.grab_focus();
    type("r");
    CHECK(tree.current_node()->text == "README.md");
}

namespace {
struct People : TableModel {
    std::vector<std::pair<std::string, int>> rows = {{"carol", 41}, {"alice", 30}, {"bob", 25}};
    size_t row_count() const override { return rows.size(); }
    std::string cell_text(size_t r, size_t c) const override { return c == 0 ? rows[r].first : std::to_string(rows[r].second); }
    bool sort(size_t c, bool asc) override {
        std::stable_sort(rows.begin(), rows.end(), [&](auto& a, auto& b) {
            bool less = c == 0 ? a.first < b.first : a.second < b.second;
            bool more = c == 0 ? b.first < a.first : b.second < a.second;
            return asc ? less : more;
        });
        return true;
    }
};
}

static void test_table() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    TableView& table = shell.content().add<TableView>();
    People model;
    table.set_columns({{"Name", 200}, {"Age", 80, 40, TextAlign::Right}, {"Notes", 300}});
    table.set_model(&model);
    relayout();
    CHECK(table.list().model()->row_count() == 3);
    CHECK(table.column_x(1) == 200 && table.column_display_width(2) >= 300);

    // Click the "Age" header: ascending, again: descending.
    float hy = 10;
    click_at(table.to_window({table.column_x(1) + 30, hy}));
    CHECK(table.sort_column() == 1 && table.sort_ascending() && model.rows[0].first == "bob");
    click_at(table.to_window({table.column_x(1) + 30, hy}));
    CHECK(!table.sort_ascending() && model.rows[0].first == "carol");
    // Drag the Name column's right edge.
    Point edge = table.to_window({200, hy});
    send(ptr(PointerEvent::Type::Move, edge));
    CHECK(shell.host().cursor() == MouseCursor::ResizeE);
    send(ptr(PointerEvent::Type::Down, edge));
    send(ptr(PointerEvent::Type::Move, {edge.x + 50, edge.y}));
    send(ptr(PointerEvent::Type::Up, {edge.x + 50, edge.y}));
    CHECK(table.columns()[0].width == 250 && table.sort_column() == 1);   // a resize is not a sort click
    // Minimum width.
    table.set_column_width(1, 5);
    CHECK(table.columns()[1].width == 40);
    // Double-click on the edge fits the content.
    Point e0 = table.to_window({table.column_x(0) + table.columns()[0].width, hy});
    click_at(e0, PointerButton::Left, 2);
    CHECK(table.columns()[0].width < 120);
    // Horizontal scrolling of a wide table (Shift+wheel and keys).
    table.set_columns({{"A", 600}, {"B", 600}});
    relayout();
    ScrollEvent se; se.pos = center(table); se.dy = 30; se.mods = mod::Shift;
    send(se);
    CHECK(table.scroll_x() == 90 && table.column_x(1) == 510);
    table.list().grab_focus();
    press(key::Right);
    CHECK(table.scroll_x() == 130);
    table.set_scroll_x(1e6f);
    CHECK(table.scroll_x() == table.content_width() - table.size().width);
    // Rows: selection and activation come from ListView.
    size_t act = 99;
    table.activated.connect([&](size_t r) { act = r; });
    click_at(table.list().to_window({20, table.list().row_height() * 1.5f}), PointerButton::Left, 2);
    CHECK(act == 1);
}

int main() {
    test_shortcuts();
    test_menus();
    test_menu_bar();
    test_dialogs();
    test_paned();
    test_expander();
    test_search_field();
    test_tree();
    test_table();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("structure tests passed\n");
    return 0;
}
