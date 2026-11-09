#include "uitk/widgets/file_browser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

#include "uitk/core/format.h"
#include "uitk/widgets/containers.h"
#include "uitk/widgets/dropdown.h"
#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/icons.h"
#include "uitk/widgets/list.h"
#include "uitk/widgets/text_field.h"
#include "uitk/widgets/toggles.h"
#include "uitk/widgets/widget_host.h"

namespace fs = std::filesystem;

namespace uitk {

// ============================================================================ helpers

namespace {

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Case-insensitive glob with '*' and '?'.
bool glob(const char* p, const char* s) {
    for (; *p; ++p, ++s) {
        if (*p == '*') {
            for (; *s; ++s) if (glob(p + 1, s)) return true;
            return glob(p + 1, s);
        }
        if (!*s || (*p != '?' && std::tolower((unsigned char)*p) != std::tolower((unsigned char)*s))) return false;
    }
    return !*s;
}

// "file2" < "file10": compare digit runs numerically, everything else case-insensitively.
bool natural_less(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (std::isdigit((unsigned char)a[i]) && std::isdigit((unsigned char)b[j])) {
            size_t i2 = i, j2 = j;
            while (i2 < a.size() && std::isdigit((unsigned char)a[i2])) ++i2;
            while (j2 < b.size() && std::isdigit((unsigned char)b[j2])) ++j2;
            std::string na = a.substr(i, i2 - i), nb = b.substr(j, j2 - j);
            na.erase(0, std::min(na.find_first_not_of('0'), na.size()));
            nb.erase(0, std::min(nb.find_first_not_of('0'), nb.size()));
            if (na.size() != nb.size()) return na.size() < nb.size();
            if (na != nb) return na < nb;
            i = i2; j = j2;
            continue;
        }
        int ca = std::tolower((unsigned char)a[i]), cb = std::tolower((unsigned char)b[j]);
        if (ca != cb) return ca < cb;
        ++i; ++j;
    }
    return a.size() - i < b.size() - j;
}

fs::path home_dir() {
    const char* h = getenv("HOME");
    return h && *h ? fs::path(h) : fs::path("/");
}

fs::path expand_user(const std::string& text) {
    if (text == "~") return home_dir();
    if (text.rfind("~/", 0) == 0) return home_dir() / text.substr(2);
    return fs::path(text);
}

std::string describe(const std::error_code& ec) {
    std::string m = ec.message();
    if (!m.empty()) m[0] = (char)std::toupper((unsigned char)m[0]);
    return m;
}

} // namespace

bool FileFilter::matches(const std::string& filename) const {
    if (patterns.empty()) return true;
    for (auto& p : patterns) if (p == "*" || glob(p.c_str(), filename.c_str())) return true;
    return false;
}

FileBrowser::Columns FileBrowser::Columns::for_width(float w) {
    Columns c;
    c.icon_x = 10;
    c.name_x = 36;
    c.date_w = 120;
    c.date_x = std::max(c.name_x + 80, w - c.date_w - 10);
    c.size_right = c.date_x - 20;
    c.name_w = std::max(40.f, c.size_right - 90 - c.name_x);
    return c;
}

// ============================================================================ model

class FileBrowser::Model : public ListModel {
public:
    explicit Model(FileBrowser& b) : b_(b) {}
    size_t row_count() const override { return b_.shown_.size(); }
    std::string row_text(size_t i) const override { return b_.shown_[i].name; }
    bool row_enabled(size_t i) const override {
        return b_.opts_.mode != FileBrowserMode::SelectFolder || b_.shown_[i].is_dir;
    }

    void paint_row(PaintContext& ctx, size_t i, const Rect& row, const RowState& st) override {
        const Theme& t = ctx.theme;
        const FileEntry& e = b_.shown_[i];
        Columns c = Columns::for_width(row.width - t.scrollbar_width);   // same geometry as the header
        bool enabled = row_enabled(i);
        Color fg = st.selected ? t.selection_fg : t.foreground;
        Color dim = st.selected ? t.selection_fg_dim : t.foreground_dim;
        if (!enabled) fg = dim = t.foreground_dim.faded(t.disabled_opacity);
        if (e.hidden && !st.selected) fg = fg.faded(0.7f);
        float cy = row.y + row.height / 2;
        ctx.canvas.draw_image(icons::for_file(e.path, e.is_dir), {row.x + c.icon_x, std::round(cy - 8), 16, 16},
                              e.is_dir ? (st.selected ? fg : t.accent) : dim);

        TextStyle s = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
        s.color = fg;
        TextLayout name(e.name + (e.is_symlink ? "  →" : ""), s, c.name_w);
        ctx.canvas.draw_text(name, {row.x + c.name_x, std::round(cy - name.size().height / 2)});

        s.color = dim;
        s.size = t.font_size_small;
        if (!e.is_dir) {
            s.align = TextAlign::Right;
            TextLayout size(format_bytes(e.size), s, 90);
            ctx.canvas.draw_text(size, {row.x + c.size_right - 90, std::round(cy - size.size().height / 2)});
        }
        if (e.modified.time_since_epoch().count() != 0) {
            s.align = TextAlign::Left;
            TextLayout date(format_date(e.modified), s, c.date_w);
            ctx.canvas.draw_text(date, {row.x + c.date_x, std::round(cy - date.size().height / 2)});
        }
    }

private:
    FileBrowser& b_;
};

// ============================================================================ column header

class FileBrowser::Header : public Widget {
public:
    explicit Header(FileBrowser& b) : b_(b) {
        name_ = &add<Button>("Name", ButtonStyle::Flat);
        size_ = &add<Button>("Size", ButtonStyle::Flat);
        date_ = &add<Button>("Modified", ButtonStyle::Flat);
        for (Button* x : {name_, size_, date_}) x->set_focusable(false);
        name_->clicked.connect([this] { toggle(SortColumn::Name); });
        size_->clicked.connect([this] { toggle(SortColumn::Size); });
        date_->clicked.connect([this] { toggle(SortColumn::Modified); });
    }
    void toggle(SortColumn c) { b_.set_sort(c, b_.sort_ == c ? !b_.ascending_ : true); }
    void update_icons() {
        for (auto [btn, col] : {std::pair{name_, SortColumn::Name}, {size_, SortColumn::Size}, {date_, SortColumn::Modified}})
            btn->set_icon(b_.sort_ == col ? glyph(b_.ascending_ ? Glyph::ChevronUp : Glyph::ChevronDown) : Image());
    }
    Size measure(float w, const Theme& t) override { return {w < 1e8f ? w : 300.f, t.control_height}; }
    void layout(const Theme& t) override {
        // Header labels line up with the row columns (the list sits below at the same x).
        Columns c = Columns::for_width(size().width - t.scrollbar_width);
        float h = size().height;
        auto place = [&](Button* btn, float x, bool right) {
            Size s = btn->measure_clamped(1e9f, t);
            btn->set_bounds({std::round(right ? x - s.width + 10 : x - 10), std::round((h - s.height) / 2), s.width, s.height});
            btn->layout(t);
        };
        place(name_, c.name_x, false);
        place(size_, c.size_right, true);
        place(date_, c.date_x, false);
    }
    void paint_over(PaintContext& ctx) override {
        ctx.canvas.draw_rect({0, size().height - 1, size().width, 1}, Paint::fill(ctx.theme.border));
    }

private:
    FileBrowser& b_;
    Button *name_, *size_, *date_;
};

// ============================================================================ FileBrowser

FileBrowser::FileBrowser(FileBrowserOptions options)
    : Box(Orientation::Vertical, 0.f), opts_(std::move(options)), model_(std::make_unique<Model>(*this)) {
    set_expand(true, true);

    // ---- toolbar ----
    Box& bar = add<Box>(Orientation::Horizontal, 2.f);
    bar.set_padding(Insets{8, 6, 8, 6});
    back_ = &bar.add<Button>(glyph(Glyph::ChevronLeft), ButtonStyle::Flat);
    forward_ = &bar.add<Button>(glyph(Glyph::ChevronRight), ButtonStyle::Flat);
    up_ = &bar.add<Button>(icons::symbolic("go-up"), ButtonStyle::Flat);
    back_->set_tooltip("Back (Alt+Left)");
    forward_->set_tooltip("Forward (Alt+Right)");
    up_->set_tooltip("Parent folder (Alt+Up)");
    back_->clicked.connect([this] { go_back(); });
    forward_->clicked.connect([this] { go_forward(); });
    up_->clicked.connect([this] { go_up(); });
    bar.add<Widget>().set_min_size({4, 1});
    path_ = &bar.add<TextField>();
    path_->set_expand(true, false);
    path_->set_tooltip("Type a path and press Enter (Ctrl+L)");
    path_->activated.connect([this] {
        if (!navigate(expand_user(path_->text()))) path_->select_all();
        else list_->grab_focus();
    });
    path_->editing_finished.connect([this] { if (!path_->has_focus()) path_->set_text(dir_.string()); });
    search_field_ = &bar.add<TextField>("", "Filter");
    search_field_->set_fixed_size({170, 0});
    search_field_->changed.connect([this](const std::string& s) { set_search(s); });
    hidden_ = &bar.add<ToggleButton>(icons::symbolic("view-reveal"), ButtonStyle::Flat);
    hidden_->set_tooltip("Show hidden files (Ctrl+H)");
    hidden_->set_checked(opts_.show_hidden);
    hidden_->toggled.connect([this](bool on) { set_show_hidden(on); });
    Button& refresh_btn = bar.add<Button>(icons::symbolic("view-refresh"), ButtonStyle::Flat);
    refresh_btn.set_tooltip("Refresh (F5)");
    refresh_btn.clicked.connect([this] { refresh(); });
    add<Separator>();

    // ---- body: places | files ----
    Box& body = add<Box>(Orientation::Horizontal, 0.f);
    body.set_expand(true, true);
    Box& side = body.add<Box>(Orientation::Vertical, 6.f);
    side.set_fixed_size({170, 0});
    side.set_padding(Insets{8, 10, 8, 8});
    side.add<Label>("Places", TextRole::Chrome).set_align(Align::Start, Align::Center);
    places_ = &side.add<ListBox>();
    fs::path home = home_dir();
    struct Place { const char* name; fs::path path; const char* icon; };
    for (const Place& p : {Place{"Home", home, "user-home"}, Place{"Desktop", home / "Desktop", "user-desktop"},
                           Place{"Documents", home / "Documents", "folder-documents"},
                           Place{"Downloads", home / "Downloads", "folder-download"},
                           Place{"Pictures", home / "Pictures", "folder-pictures"},
                           Place{"Music", home / "Music", "folder-music"},
                           Place{"Videos", home / "Videos", "folder-videos"},
                           Place{"Computer", fs::path("/"), "drive-harddisk"}}) {
        std::error_code ec;
        if (!fs::is_directory(p.path, ec)) continue;
        ListRow& row = places_->add<ListRow>(p.name, icons::symbolic(p.icon));
        size_t index = place_paths_.size();
        place_paths_.push_back(p.path);
        row.activated.connect([this, index] { navigate(place_paths_[index]); });
    }
    body.add<Separator>(Orientation::Vertical);

    Box& main = body.add<Box>(Orientation::Vertical, 0.f);
    main.set_expand(true, true);
    header_ = &main.add<Header>(*this);
    scroll_ = &main.add<ScrollView>();
    list_ = &scroll_->set_content<ListView>();
    list_->set_selection_mode(opts_.mode == FileBrowserMode::OpenFiles ? SelectionMode::Multiple : SelectionMode::Single);
    list_->set_model(model_.get());
    list_->activated.connect([this](size_t i) { activate(i); });
    list_->selection_changed.connect([this] { update_status(); selection_changed.emit(); });
    add<Separator>();

    // ---- footer ----
    Box& foot = add<Box>(Orientation::Horizontal, 8.f);
    foot.set_padding(Insets{10, 6, 8, 6});
    status_ = &foot.add<Label>("", TextRole::Dim);
    status_->set_align(Align::Fill, Align::Center);
    status_->set_ellipsize(true);
    status_->set_expand(true, false);
    if (!opts_.filters.empty()) {
        std::vector<std::string> names;
        for (auto& f : opts_.filters) names.push_back(f.name);
        filter_box_ = &foot.add<Dropdown>(names, 0);
        filter_box_->changed.connect([this](int i) { set_filter(i); });
    }

    header_->update_icons();
    fs::path start = opts_.start_directory.empty() ? home : opts_.start_directory;
    if (!navigate(start)) navigate(home);
}

FileBrowser::~FileBrowser() { list_->set_model(nullptr); }

// ---------------------------------------------------------------------------- navigation

bool FileBrowser::navigate(const fs::path& target) {
    std::error_code ec;
    fs::path p = fs::weakly_canonical(fs::absolute(target, ec), ec);
    if (ec || !fs::is_directory(p, ec)) {
        error_ = "“" + target.string() + "” is not a folder.";
        update_status();
        return false;
    }
    // Check readability before leaving the current folder.
    fs::directory_iterator probe(p, ec);
    if (ec) {
        error_ = "Cannot open “" + p.filename().string() + "”: " + describe(ec);
        update_status();
        return false;
    }
    if (p != dir_) {
        if (!history_.empty()) history_.resize(history_index_ + 1);
        history_.push_back(p);
        history_index_ = history_.size() - 1;
    }
    dir_ = p;
    error_.clear();
    search_.clear();
    search_field_->set_text("");
    load();
    directory_changed.emit(dir_);
    return true;
}

void FileBrowser::go_back() {
    if (history_index_ == 0) return;
    --history_index_;
    dir_ = history_[history_index_];
    load();
    directory_changed.emit(dir_);
}

void FileBrowser::go_forward() {
    if (history_index_ + 1 >= history_.size()) return;
    ++history_index_;
    dir_ = history_[history_index_];
    load();
    directory_changed.emit(dir_);
}

void FileBrowser::go_up() {
    if (dir_.has_parent_path() && dir_.parent_path() != dir_) {
        fs::path from = dir_;
        if (navigate(dir_.parent_path())) {
            // Select the folder we came from, like file managers do.
            for (size_t i = 0; i < shown_.size(); ++i)
                if (shown_[i].path == from) { list_->select_only(i); list_->set_current(i); break; }
        }
    }
}

void FileBrowser::refresh() { load(); }

void FileBrowser::load() {
    all_.clear();
    std::error_code ec;
    for (fs::directory_iterator it(dir_, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end;
         it.increment(ec)) {
        const fs::directory_entry& de = *it;
        FileEntry e;
        e.path = de.path();
        e.name = e.path.filename().string();
        e.hidden = !e.name.empty() && e.name[0] == '.';
        std::error_code ec2;
        e.is_symlink = de.is_symlink(ec2);
        e.is_dir = de.is_directory(ec2);   // follows symlinks
        if (!e.is_dir) {
            uint64_t sz = de.is_regular_file(ec2) ? de.file_size(ec2) : 0;
            e.size = ec2 ? 0 : sz;
        }
        auto ft = de.last_write_time(ec2);
        if (!ec2) e.modified = std::chrono::clock_cast<std::chrono::system_clock>(ft);
        all_.push_back(std::move(e));
    }
    if (ec) error_ = "Some items could not be read: " + describe(ec);
    apply_view();
}

// ---------------------------------------------------------------------------- view

void FileBrowser::set_show_hidden(bool show) {
    if (show == opts_.show_hidden) return;
    opts_.show_hidden = show;
    hidden_->set_checked(show);
    apply_view();
}

void FileBrowser::set_filter(int index) {
    if (index < 0 || index >= (int)opts_.filters.size()) return;
    filter_ = index;
    if (filter_box_) filter_box_->set_selected(index);
    apply_view();
}

void FileBrowser::set_search(const std::string& text) {
    if (text == search_) return;
    search_ = text;
    apply_view();
}

void FileBrowser::set_sort(SortColumn column, bool ascending) {
    sort_ = column;
    ascending_ = ascending;
    header_->update_icons();
    apply_view();
}

void FileBrowser::apply_view() {
    // Remember the selection by path so it survives re-sorting / filtering.
    std::vector<fs::path> keep = selected_paths();
    std::string needle = lower(search_);
    const FileFilter* filter = opts_.filters.empty() ? nullptr : &opts_.filters[(size_t)filter_];
    shown_.clear();
    for (const FileEntry& e : all_) {
        if (e.hidden && !opts_.show_hidden) continue;
        if (!needle.empty() && lower(e.name).find(needle) == std::string::npos) continue;
        if (!e.is_dir && filter && !filter->matches(e.name)) continue;
        shown_.push_back(e);
    }
    // Strict ordering by the sort column (name as tie-breaker); reversed for descending.
    auto key_less = [this](const FileEntry& a, const FileEntry& b) {
        switch (sort_) {
            case SortColumn::Size:     if (a.size != b.size) return a.size < b.size; break;
            case SortColumn::Modified: if (a.modified != b.modified) return a.modified < b.modified; break;
            case SortColumn::Name:     break;
        }
        return natural_less(a.name, b.name);
    };
    std::stable_sort(shown_.begin(), shown_.end(), [&](const FileEntry& a, const FileEntry& b) {
        if (a.is_dir != b.is_dir) return a.is_dir;   // folders first, in both directions
        return ascending_ ? key_less(a, b) : key_less(b, a);
    });
    model_->changed.emit();
    for (size_t i = 0; i < shown_.size(); ++i)
        if (std::find(keep.begin(), keep.end(), shown_[i].path) != keep.end()) list_->select_only(i);
    scroll_->scroll_to(0, false);
    update_chrome();
    update_status();
}

void FileBrowser::update_chrome() {
    back_->set_enabled(history_index_ > 0);
    forward_->set_enabled(history_index_ + 1 < history_.size());
    up_->set_enabled(dir_.has_parent_path() && dir_.parent_path() != dir_);
    if (!path_->has_focus()) path_->set_text(dir_.string());
    // Highlight the place we are in (if any).
    ListRow* match = nullptr;
    size_t k = 0;
    for (auto& c : places_->children())
        if (auto* row = dynamic_cast<ListRow*>(c.get())) {
            if (place_paths_[k] == dir_) match = row;
            ++k;
        }
    places_->select(match);
    list_->set_placeholder(!search_.empty() ? "No items match “" + search_ + "”." : "This folder is empty.");
}

void FileBrowser::update_status() {
    if (!error_.empty()) {
        status_->set_color_token(ColorToken::Error);
        status_->set_text(error_);
        return;
    }
    status_->set_color_token(std::nullopt);
    size_t folders = 0;
    for (auto& e : shown_) folders += e.is_dir;
    std::string s = std::to_string(shown_.size()) + (shown_.size() == 1 ? " item" : " items");
    if (folders && folders != shown_.size()) s += " (" + std::to_string(folders) + (folders == 1 ? " folder)" : " folders)");
    auto sel = list_->selection();
    if (!sel.empty()) {
        uint64_t bytes = 0;
        for (size_t i : sel) if (!shown_[i].is_dir) bytes += shown_[i].size;
        s += "  ·  " + std::to_string(sel.size()) + " selected";
        if (bytes) s += " (" + format_bytes(bytes) + ")";
    }
    status_->set_text(s);
}

std::vector<fs::path> FileBrowser::selected_paths() const {
    std::vector<fs::path> out;
    for (size_t i : list_->selection()) if (i < shown_.size()) out.push_back(shown_[i].path);
    return out;
}

bool FileBrowser::can_accept() const {
    auto sel = list_->selection();
    if (opts_.mode == FileBrowserMode::SelectFolder) return true;   // the selected or current folder
    return std::any_of(sel.begin(), sel.end(), [this](size_t i) { return i < shown_.size() && !shown_[i].is_dir; });
}

std::vector<fs::path> FileBrowser::accept_paths() const {
    std::vector<fs::path> out;
    for (size_t i : list_->selection()) {
        if (i >= shown_.size()) continue;
        bool want_dir = opts_.mode == FileBrowserMode::SelectFolder;
        if (shown_[i].is_dir == want_dir) out.push_back(shown_[i].path);
    }
    if (out.empty() && opts_.mode == FileBrowserMode::SelectFolder) out.push_back(dir_);
    return out;
}

void FileBrowser::activate(size_t row) {
    if (row >= shown_.size()) return;
    if (shown_[row].is_dir) { navigate(shown_[row].path); return; }
    if (opts_.mode == FileBrowserMode::SelectFolder) return;
    if (!list_->is_selected(row)) list_->select_only(row);
    std::vector<fs::path> files = accept_paths();
    if (!files.empty()) files_activated.emit(files);
}

bool FileBrowser::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    bool alt = e.mods & MOD_ALT, ctrl = e.mods & MOD_CTRL;
    if (alt && e.keysym == key::Left) { go_back(); return true; }
    if (alt && e.keysym == key::Right) { go_forward(); return true; }
    if (alt && e.keysym == key::Up) { go_up(); return true; }
    if (e.keysym == key::BackSpace && host() && host()->focus() == list_) { go_up(); return true; }
    if (ctrl && e.keysym == key::letter('l')) { path_->grab_focus(); path_->select_all(); return true; }
    if (ctrl && e.keysym == key::letter('h')) { set_show_hidden(!opts_.show_hidden); return true; }
    if (e.keysym == 0xffc2 /* F5 */) { refresh(); return true; }
    return false;
}

// ============================================================================ FileDialog

FileDialog::FileDialog(std::string title, FileBrowserOptions options) : Dialog(std::move(title)) {
    bool folder = options.mode == FileBrowserMode::SelectFolder;
    set_preferred_size({900, 600});
    set_section_separators(true);
    body().set_padding(0);
    browser_ = &body().add<FileBrowser>(std::move(options));
    browser_->set_expand(true, true);
    add_button("Cancel", Dialog::Cancel);
    accept_button_ = &add_button(folder ? "Select Folder" : "Open", Dialog::Ok, ButtonStyle::Primary);
    // Not respond(Ok) directly: accept() collects the paths first.
    accept_button_->clicked.disconnect_all();
    accept_button_->clicked.connect([this] { accept(); });
    auto update = [this] { accept_button_->set_enabled(browser_->can_accept()); };
    browser_->selection_changed.connect(update);
    browser_->directory_changed.connect([update](const std::filesystem::path&) { update(); });
    browser_->files_activated.connect([this](const std::vector<std::filesystem::path>&) { accept(); });
    update();
}

void FileDialog::accept() {
    if (!browser_->can_accept()) return;
    auto paths = browser_->accept_paths();
    accepted_ = true;
    auto sig = accepted;   // respond() destroys the dialog
    respond(Dialog::Ok);
    sig.emit(paths);
}

FileDialog& FileDialog::open(WidgetHost& host, std::string title, FileBrowserOptions options,
                             std::function<void(const std::vector<std::filesystem::path>&)> on_accept,
                             std::function<void()> on_cancel) {
    FileDialog& d = Dialog::show<FileDialog>(host, std::move(title), std::move(options));
    if (on_accept) d.accepted.connect(std::move(on_accept));
    if (on_cancel) d.responded.connect([on_cancel](int r) { if (r != Dialog::Ok) on_cancel(); });
    d.browser().list().grab_focus();
    return d;
}

} // namespace uitk
