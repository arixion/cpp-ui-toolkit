// "workbench" example: an IDE-style window built from the application-structure controls.
//
//   header    MenuBar (File / Edit / View / Help, shortcuts, submenus, check + radio items)
//   left      Accordion side bar: Explorer (TreeView over a real folder, lazy loading,
//             context menu) + collapsed sections
//   right     Paned: TableView of the selected folder (sortable, resizable, SearchField filter)
//             over a settings panel of Expanders
//   dialogs   New File / Rename (text input), Delete (confirmation), About, Open Folder
//
// The demo never changes files on disk: rename / delete / new file only change the views.
//
//   workbench_cpu [--dir PATH]
//   workbench_cpu --snapshot out.png [--dark] [--menu] [--context] [--dialog] [--size WxH] [--dir PATH]
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "uitk/core/application.h"
#include "uitk/core/format.h"
#include "uitk/render/renderer.h"
#include "uitk/widgets/application_window.h"
#include "uitk/widgets/containers.h"
#include "uitk/widgets/dialog.h"
#include "uitk/widgets/expander.h"
#include "uitk/widgets/file_browser.h"
#include "uitk/widgets/icons.h"
#include "uitk/widgets/menu.h"
#include "uitk/widgets/paned.h"
#include "uitk/widgets/search_field.h"
#include "uitk/widgets/status_bar.h"
#include "uitk/widgets/table_view.h"
#include "uitk/widgets/text_field.h"
#include "uitk/widgets/toggles.h"
#include "uitk/widgets/tree_view.h"

using namespace uitk;
namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// ---------------------------------------------------------------------------- folder table

struct FileRow {
    fs::path path;
    std::string name;
    bool dir = false;
    uint64_t size = 0;
    std::chrono::system_clock::time_point modified{};
};

std::string type_of(const FileRow& r) {
    if (r.dir) return "Folder";
    std::string ext = r.path.extension().string();
    if (ext.size() < 2) return "File";
    std::string e = ext.substr(1);
    for (auto& c : e) c = (char)std::toupper((unsigned char)c);
    return e + " file";
}

class FolderModel : public TableModel {
public:
    void load(const fs::path& dir) {
        all_.clear();
        std::error_code ec;
        for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::directory_iterator(); it.increment(ec)) {
            FileRow r;
            r.path = it->path();
            r.name = r.path.filename().string();
            if (!r.name.empty() && r.name[0] == '.') continue;
            std::error_code e2;
            r.dir = it->is_directory(e2);
            if (!r.dir) r.size = it->file_size(e2);
            auto ft = it->last_write_time(e2);
            if (!e2) r.modified = std::chrono::clock_cast<std::chrono::system_clock>(ft);
            all_.push_back(std::move(r));
        }
        apply();
    }
    void set_filter(std::string f) { filter_ = lower(std::move(f)); apply(); }
    const FileRow& row(size_t i) const { return rows_[i]; }
    size_t total() const { return all_.size(); }

    size_t row_count() const override { return rows_.size(); }
    std::string cell_text(size_t r, size_t c) const override {
        const FileRow& f = rows_[r];
        switch (c) {
            case 0: return f.name;
            case 1: return type_of(f);
            case 2: return f.dir ? std::string() : format_bytes(f.size);
            default: return format_date(f.modified, std::chrono::system_clock::now());
        }
    }
    void paint_cell(PaintContext& ctx, size_t r, size_t c, const Rect& cell, const RowState& st, const TableColumn& col) override {
        if (c != 0) { TableModel::paint_cell(ctx, r, c, cell, st, col); return; }
        const Theme& t = ctx.theme;
        const FileRow& f = rows_[r];
        Color dim = st.selected && st.focused ? t.selection_fg_dim : t.foreground_dim;
        ctx.canvas.draw_image(icons::for_file(f.path, f.dir), {cell.x + 8, cell.y + std::round((cell.height - 16) / 2), 16, 16}, dim);
        Rect rest{cell.x + 22, cell.y, cell.width - 22, cell.height};
        TableModel::paint_cell(ctx, r, c, rest, st, col);
    }
    bool sort(size_t c, bool asc) override {
        sort_col_ = c;
        asc_ = asc;
        apply();
        return true;
    }

private:
    void apply() {
        rows_.clear();
        for (auto& r : all_) if (filter_.empty() || lower(r.name).find(filter_) != std::string::npos) rows_.push_back(r);
        auto key_less = [&](const FileRow& a, const FileRow& b) {
            switch (sort_col_) {
                case 1: return type_of(a) < type_of(b);
                case 2: return a.size < b.size;
                case 3: return a.modified < b.modified;
                default: return lower(a.name) < lower(b.name);
            }
        };
        std::stable_sort(rows_.begin(), rows_.end(), [&](const FileRow& a, const FileRow& b) {
            if (a.dir != b.dir) return a.dir;   // folders first
            return asc_ ? key_less(a, b) : key_less(b, a);
        });
        changed.emit();
    }

    std::vector<FileRow> all_, rows_;
    std::string filter_;
    size_t sort_col_ = 0;
    bool asc_ = true;
};

// ---------------------------------------------------------------------------- explorer tree

fs::path node_path(const TreeNode& n) {
    const fs::path* p = std::any_cast<fs::path>(&n.data);
    return p ? *p : fs::path();
}

void load_children(TreeNode& n) {
    fs::path dir = node_path(n);
    std::vector<fs::directory_entry> entries;
    std::error_code ec;
    for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::directory_iterator() && entries.size() < 5000; it.increment(ec)) {
        std::string name = it->path().filename().string();
        if (!name.empty() && name[0] != '.') entries.push_back(*it);
    }
    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
        std::error_code e;
        bool da = a.is_directory(e), db = b.is_directory(e);
        if (da != db) return da;
        return lower(a.path().filename().string()) < lower(b.path().filename().string());
    });
    for (auto& e : entries) {
        std::error_code e2;
        bool d = e.is_directory(e2);
        TreeNode& c = n.add(e.path().filename().string(), icons::for_file(e.path(), d));
        c.data = e.path();
        c.set_lazy(d);
    }
    n.set_lazy(false);
}

// ---------------------------------------------------------------------------- the window

struct Workbench {
    WidgetHost* host = nullptr;
    std::function<void(const Theme&)> set_theme;
    std::function<void()> quit;
    MenuBar* bar = nullptr;
    Paned* split = nullptr;
    Accordion* side = nullptr;
    TreeView* tree = nullptr;
    Paned* main_split = nullptr;
    Widget* panel = nullptr;
    Label* folder_title = nullptr;
    SearchField* search = nullptr;
    TableView* table = nullptr;
    FolderModel model;
    Button* status_path = nullptr;
    Button* status_count = nullptr;
    fs::path folder;

    void open_root(const fs::path& dir) {
        tree->root().clear();
        TreeNode& r = tree->root().add(dir.filename().empty() ? dir.string() : dir.filename().string(), icons::symbolic("folder"));
        r.data = dir;
        r.set_lazy(true);
        tree->refresh();
        tree->expand(r);
        tree->select(r);
        show_folder(dir);
    }
    void show_folder(const fs::path& dir) {
        folder = dir;
        model.load(dir);
        folder_title->set_text(dir.filename().empty() ? dir.string() : dir.filename().string());
        status_path->set_label(dir.string());
        update_count();
    }
    void update_count() {
        size_t sel = table->list().selection().size();
        std::string s = std::to_string(model.row_count()) + " of " + std::to_string(model.total()) + " items";
        if (sel) s += " · " + std::to_string(sel) + " selected";
        status_count->set_label(s);
    }
    void status(const std::string& msg) { status_path->set_label(msg); }

    // Text input dialog (New File / Rename).
    void ask_name(const std::string& title, const std::string& action, const std::string& initial,
                  std::function<void(const std::string&)> done) {
        Dialog& d = Dialog::show<Dialog>(*host, title);
        d.body().add<Label>("Name", TextRole::Dim).set_align(Align::Start, Align::Center);
        TextField& f = d.body().add<TextField>(initial, "file name");
        f.set_min_size({340, 0});
        Label& hint = d.body().add<Label>("Only the view changes: files on disk are not touched in this demo.", TextRole::Caption);
        hint.set_wrap(true);
        d.add_button("Cancel", Dialog::Cancel);
        Button& ok = d.add_button(action, Dialog::Ok, ButtonStyle::Primary);
        d.set_default_response(Dialog::Ok);
        ok.set_enabled(!initial.empty());
        f.changed.connect([&ok](const std::string& t) { ok.set_enabled(!t.empty()); });
        f.grab_focus();
        f.select_all();
        // The dialog (and the field) are gone when `responded` fires: keep the text outside.
        auto text = std::make_shared<std::string>(initial);
        f.changed.connect([text](const std::string& t) { *text = t; });
        d.responded.connect([text, done](int r) { if (r == Dialog::Ok && !text->empty()) done(*text); });
    }

    std::shared_ptr<Menu> node_menu(TreeNode* n) {
        auto m = std::make_shared<Menu>();
        if (!n) return m;
        fs::path p = node_path(*n);
        bool dir = n->has_children() || fs::is_directory(p);
        if (dir) m->add("&Open in Table", [this, p] { show_folder(p); });
        m->add("&New File…", [this, n, dir] {
            TreeNode* parent = dir ? n : n->parent();
            ask_name("New File", "Create", "untitled.txt", [this, parent](const std::string& name) {
                TreeNode& c = parent->add(name, icons::for_file(name, false));
                c.data = node_path(*parent) / name;
                tree->expand(*parent);
                tree->refresh();
                tree->select(c);
                status("Added “" + name + "” to the view");
            });
        }).set_icon(icons::symbolic("document-new"));
        m->add("Re&name…", [this, n] {
            ask_name("Rename", "Rename", n->text, [this, n](const std::string& name) {
                std::string old = n->text;
                n->text = name;
                tree->refresh();
                status("Renamed “" + old + "” to “" + name + "” in the view");
            });
        }, Shortcut::parse("F2")).set_icon(icons::symbolic("document-edit"));
        m->add_separator();
        m->add("Copy &Path", [this, p] { status("Path: " + p.string()); }, Shortcut::parse("Ctrl+Shift+C"))
            .set_icon(icons::symbolic("edit-copy"));
        if (n->parent() && n->parent() != &tree->root()) {
            m->add_separator();
            m->add("&Delete", [this, n] {
                std::string name = n->text;
                Dialog::message(*host, MessageKind::Warning, "Delete “" + name + "”?",
                                "It is only removed from this view; the demo never deletes files.",
                                {{"Cancel", Dialog::Cancel}, {"Delete", Dialog::Ok, ButtonStyle::Destructive}},
                                [this, n, name](int r) {
                                    if (r != Dialog::Ok) return;
                                    n->parent()->remove(n);
                                    tree->refresh();
                                    status("Removed “" + name + "” from the view");
                                });
            }, Shortcut::parse("Delete")).set_icon(icons::symbolic("edit-delete"));
        }
        return m;
    }

    void build_menus(Box& slot) {
        bar = &slot.add<MenuBar>();
        Menu& file = bar->add_menu("&File");
        file.add("&New File…", [this] { if (TreeNode* n = tree->current_node()) node_menu(n)->activate(1); }, Shortcut::parse("Ctrl+N"))
            .set_icon(icons::symbolic("document-new"));
        file.add("&Open Folder…", [this] {
            FileBrowserOptions o;
            o.mode = FileBrowserMode::SelectFolder;
            o.start_directory = folder.parent_path();
            FileDialog::open(*host, "Open Folder", o, [this](const std::vector<fs::path>& p) { if (!p.empty()) open_root(p.front()); });
        }, Shortcut::parse("Ctrl+O")).set_icon(icons::symbolic("folder-open"));
        Menu& recent = file.add_submenu("Open &Recent");
        for (const char* d : {"/usr/share/icons", "/etc", "/usr/include"})
            if (fs::is_directory(d)) recent.add(d, [this, p = fs::path(d)] { open_root(p); });
        if (const char* home = getenv("HOME")) recent.add("~ (home)", [this, p = fs::path(home)] { open_root(p); });
        file.add_separator();
        file.add("E&xit", [this] { if (quit) quit(); }, Shortcut::parse("Ctrl+Q"));

        Menu& edit = bar->add_menu("&Edit");
        edit.add("&Undo", {}, Shortcut::parse("Ctrl+Z")).set_enabled(false);
        edit.add("&Redo", {}, Shortcut::parse("Ctrl+Shift+Z")).set_enabled(false);
        edit.add_separator();
        edit.add("&Find", [this] { search->field().grab_focus(); search->field().select_all(); }, Shortcut::parse("Ctrl+F"))
            .set_icon(icons::symbolic("edit-find"));
        edit.add("Select &All Rows", [this] { table->list().select_all(); }, Shortcut::parse("Ctrl+Shift+A"));

        Menu& view = bar->add_menu("&View");
        view.add_check("&Side Bar", true, [this](bool on) { side->set_visible(on); }, Shortcut::parse("Ctrl+B"));
        view.add_check("&Panel", true, [this](bool on) { panel->set_visible(on); }, Shortcut::parse("Ctrl+J"));
        view.add_separator();
        view.add_check("S&triped Rows", true, [this](bool on) { table->set_striped(on); });
        view.add_check("&Grid Lines", false, [this](bool on) { table->set_grid_lines(on); });
        view.add_separator();
        Menu& themes = view.add_submenu("&Theme");
        struct T { const char* name; Theme (*make)(); };
        static const T kThemes[] = {{"VS Code &Dark", Theme::vscode_dark}, {"VS Code &Light", Theme::vscode_light},
                                    {"&Adwaita Dark", Theme::adwaita_dark}, {"Adwaita L&ight", Theme::adwaita_light}};
        for (int i = 0; i < 4; ++i)
            themes.add_radio(1, kThemes[i].name, i == 0, [this, i] { if (set_theme) set_theme(kThemes[i].make()); });

        Menu& help = bar->add_menu("&Help");
        help.add("&Keyboard Shortcuts", [this] {
            Dialog::message(*host, MessageKind::Info, "Keyboard shortcuts",
                            "Alt+F/E/V/H open the menus, F10 the first one. Ctrl+B side bar, Ctrl+J panel, "
                            "Ctrl+F find, Ctrl+O open folder. Menu key or Shift+F10 opens the context menu "
                            "of the focused list. Tree: Left/Right collapse/expand.");
        }, Shortcut::parse("F1"));
        help.add_separator();
        help.add("&About", [this] {
            Dialog::message(*host, MessageKind::Info, "uitk workbench",
                            "Menus, dialogs, splitters, expanders, search, tree and table views — all drawn "
                            "by the toolkit (no native widgets).");
        });
    }

    void build(WindowShell& shell, const fs::path& start) {
        host = &shell.host();
        build_menus(shell.header_bar().start());

        split = &shell.content().add<Paned>(Orientation::Horizontal);
        split->set_position(270);
        split->set_min_sizes(170, 360);

        // ---- side bar ----
        side = &split->set_start<Accordion>();
        Expander& explorer = side->add_section("Explorer", true);
        explorer.set_fill(true);
        Button& collapse = explorer.header_actions().add<Button>(icons::symbolic("view-restore"), ButtonStyle::Flat);
        collapse.set_tooltip("Collapse Folders");
        collapse.set_fixed_size({22, 22});
        Button& refresh = explorer.header_actions().add<Button>(icons::symbolic("view-refresh"), ButtonStyle::Flat);
        refresh.set_tooltip("Refresh Explorer");
        refresh.set_fixed_size({22, 22});
        tree = &explorer.set_content<TreeView>();
        tree->set_placeholder("No folder opened");
        tree->expanding.connect([](TreeNode& n) { if (n.lazy()) load_children(n); });
        tree->selection_changed.connect([this] {
            TreeNode* n = tree->current_node();
            if (!n) return;
            fs::path p = node_path(*n);
            std::error_code ec;
            if (fs::is_directory(p, ec)) show_folder(p);
            else status(p.string());
        });
        tree->set_context_menu_provider([this](TreeNode* n) { return node_menu(n); });
        collapse.clicked.connect([this] { tree->collapse_all(); });
        refresh.clicked.connect([this] { open_root(node_path(*tree->root().children().front())); });
        Expander& outline = side->add_section("Outline");
        outline.content().set_padding(Insets{20, 6, 12, 10});
        outline.content().add<Label>("The active editor has no outline.", TextRole::Dim).set_wrap(true);
        Expander& timeline = side->add_section("Timeline");
        timeline.content().set_padding(Insets{20, 6, 12, 10});
        timeline.content().add<Label>("No timeline information.", TextRole::Dim).set_wrap(true);

        // ---- main: table over a settings panel ----
        main_split = &split->set_end<Paned>(Orientation::Vertical);
        main_split->set_resize_policy(PanedResize::KeepEnd);
        main_split->set_min_sizes(160, 90);
        Box& top = main_split->set_start<Box>(Orientation::Vertical, 0.f);
        Box& tools = top.add<Box>(Orientation::Horizontal, 8.f);
        tools.set_padding(Insets{14, 8, 10, 8});
        folder_title = &tools.add<Label>("", TextRole::Button);
        folder_title->set_align(Align::Start, Align::Center);
        folder_title->set_expand(true, false);
        folder_title->set_ellipsize(true);
        search = &tools.add<SearchField>("Filter files");
        search->set_fixed_size({260, 0});
        search->set_debounce(120);
        search->set_history_enabled(true);
        search->search_changed.connect([this](const std::string& q) { model.set_filter(q); update_count(); });
        table = &top.add<TableView>();
        table->set_columns({{"Name", 280}, {"Type", 110}, {"Size", 90, 50, TextAlign::Right}, {"Modified", 130}});
        table->set_model(&model);
        table->set_selection_mode(SelectionMode::Multiple);
        table->set_striped(true);
        table->list().set_placeholder("No matching files");
        table->selection_changed.connect([this] { update_count(); });
        table->activated.connect([this](size_t r) {
            const FileRow& f = model.row(r);
            if (f.dir) show_folder(f.path); else status("Opened " + f.path.string() + " (demo)");
        });
        auto row_menu = std::make_shared<Menu>();
        row_menu->add("Copy &Name", [this] {
            size_t c = table->list().current();
            if (c < model.row_count()) status("Name: " + model.row(c).name);
        }).set_icon(icons::symbolic("edit-copy"));
        row_menu->add("Show in &Explorer", [this] {
            size_t c = table->list().current();
            if (c >= model.row_count()) return;
            fs::path p = model.row(c).path;
            // Walk the tree from the root, loading folders on the way.
            TreeNode* n = tree->root().children().empty() ? nullptr : tree->root().children().front().get();
            fs::path base = n ? node_path(*n) : fs::path();
            fs::path rel = p.lexically_relative(base);
            for (auto& part : rel) {
                if (!n || part == "." || part.empty()) break;
                if (n->lazy()) load_children(*n);
                TreeNode* next = nullptr;
                for (auto& ch : n->children()) if (ch->text == part.string()) { next = ch.get(); break; }
                n = next;
            }
            if (n) { tree->select(*n); tree->list().grab_focus(); }
        });
        table->set_context_menu(row_menu);

        // Settings panel: plain expanders.
        ScrollView& panel_scroll = main_split->set_end<ScrollView>();
        panel = &panel_scroll;
        Box& settings = panel_scroll.set_content<Box>(Orientation::Vertical, 2.f);
        settings.set_padding(Insets{12, 8, 16, 8});
        Expander& ed = settings.add<Expander>("Table", true);
        CheckBox& striped = ed.content().add<CheckBox>("Striped rows");
        striped.set_checked(true);
        striped.toggled.connect([this](bool on) { table->set_striped(on); });
        CheckBox& grid = ed.content().add<CheckBox>("Grid lines");
        grid.toggled.connect([this](bool on) { table->set_grid_lines(on); });
        Expander& tr = settings.add<Expander>("Explorer");
        Switch& click_open = tr.content().add<Switch>("Single click opens folders");
        click_open.set_checked(true);
        click_open.toggled.connect([this](bool on) { tree->set_expand_on_click(on); });
        Expander& ab = settings.add<Expander>("About this panel");
        Label& about = ab.content().add<Label>("Drag the dividers to resize the side bar and this panel; "
                                              "double-click a divider to restore it.", TextRole::Dim);
        about.set_wrap(true);
        main_split->set_position(440);

        // ---- status bar ----
        StatusBar& sb = shell.content().add<StatusBar>();
        status_path = &sb.add_start("", icons::symbolic("folder"));
        status_count = &sb.add_end("");

        open_root(start);
    }
};

int snapshot(const char* out, bool dark, bool menu, bool context, bool dialog, Size size, const fs::path& dir) {
    WindowShell shell(nullptr, "uitk workbench");
    shell.set_theme(dark ? Theme::vscode_dark() : Theme::vscode_light(), false);
    Workbench wb;
    wb.build(shell, dir);
    shell.host().layout(size);
    if (wb.tree->row_count() > 3) { wb.tree->list().select_only(2); wb.tree->list().grab_focus(); }
    if (wb.model.row_count() > 2) wb.table->list().set_selection({1, 2}, 2);
    shell.host().layout(size);
    if (menu) {
        wb.bar->open(0, true);
        shell.host().layout(size);
        if (auto* m = dynamic_cast<MenuPopup*>(shell.host().top_popup())) { m->set_highlighted(2); m->open_submenu(2, true); }
    } else if (context) {
        Rect a = wb.tree->list().keyboard_anchor();
        shell.host().open_context_menu(&wb.tree->list(), wb.tree->list().to_window({a.x + 60, a.center().y}));
    } else if (dialog) {
        if (TreeNode* n = wb.tree->current_node()) wb.node_menu(n)->activate(2);   // Rename…
    }
    shell.host().layout(size);
    Insets m = WindowFrame::shadow_insets();
    auto r = OffscreenRenderer::create({size.width + m.left + m.right, size.height + m.top + m.bottom}, 1.f);
    Canvas* c = r->begin_frame();
    c->translate(m.left, m.top);
    shell.paint(*c, size);
    r->end_frame();
    if (!r->write_png(out)) return 1;
    printf("%s\n", out);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const char* snap = nullptr;
    bool dark = false, menu = false, context = false, dialog = false;
    Size size{1280, 780};
    fs::path dir = fs::current_path();
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--snapshot") && i + 1 < argc) snap = argv[++i];
        else if (!strcmp(argv[i], "--dark")) dark = true;
        else if (!strcmp(argv[i], "--menu")) menu = true;
        else if (!strcmp(argv[i], "--context")) context = true;
        else if (!strcmp(argv[i], "--dialog")) dialog = true;
        else if (!strcmp(argv[i], "--dir") && i + 1 < argc) dir = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            int w, h; if (sscanf(argv[++i], "%dx%d", &w, &h) == 2) size = {(float)w, (float)h};
        }
    }
    if (snap) return snapshot(snap, dark, menu, context, dialog, size, dir);

    Application app;
    WindowDesc desc;
    desc.title = "uitk workbench";
    desc.app_id = "uitk-workbench";
    desc.size = size;
    ApplicationWindow win(app, desc);
    win.set_theme(Theme::vscode_dark(), false);
    Workbench wb;
    wb.set_theme = [&win](const Theme& t) { win.set_theme(t); };
    wb.quit = [&app] { app.quit(); };
    wb.build(win.shell(), dir);
    return app.run();
}
