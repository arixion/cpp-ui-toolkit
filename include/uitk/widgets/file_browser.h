// FileBrowser: an in-app file browser (no native dialog), built from existing widgets.
//
//   toolbar   back / forward / up, editable path, live name filter, hidden-files toggle, refresh
//   sidebar   places (Home, Desktop, Documents, Downloads, ... , Computer)
//   list      virtualized ListView with sortable Name / Size / Modified columns, type icons,
//             multi-selection, type-ahead, double-click / Enter to open
//   footer    item / selection summary, errors, file-type filter dropdown
//
// Keyboard: Alt+Left/Right history, Alt+Up or Backspace parent folder, Ctrl+L edit the path,
// Ctrl+H hidden files, F5 refresh, Ctrl+A select all (multiple mode).
//
// FileDialog wraps it in a modal in-window dialog with Cancel / Open.
#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/button.h"
#include "uitk/widgets/dialog.h"
#include "uitk/widgets/frame.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/list_view.h"

namespace uitk {

class Dropdown;
class ListBox;
class ScrollView;
class TextField;
class ToggleButton;
class WidgetHost;

struct FileFilter {
    std::string name;                    // "Images"
    std::vector<std::string> patterns;   // {"*.png", "*.jpg"}; "*" matches everything
    bool matches(const std::string& filename) const;   // case-insensitive glob (* and ?)
};

enum class FileBrowserMode { OpenFile, OpenFiles, SelectFolder };

struct FileBrowserOptions {
    FileBrowserMode mode = FileBrowserMode::OpenFile;
    std::filesystem::path start_directory;   // empty: home
    std::vector<FileFilter> filters;         // empty: all files
    bool show_hidden = false;
};

struct FileEntry {
    std::filesystem::path path;
    std::string name;
    bool is_dir = false, is_symlink = false, hidden = false;
    uint64_t size = 0;
    std::chrono::system_clock::time_point modified{};
};

enum class SortColumn { Name, Size, Modified };

class FileBrowser : public Box {
public:
    explicit FileBrowser(FileBrowserOptions options = {});
    ~FileBrowser() override;

    bool navigate(const std::filesystem::path& directory);   // false (and an error shown) on failure
    void go_back();
    void go_forward();
    void go_up();
    void refresh();
    const std::filesystem::path& directory() const { return dir_; }

    void set_show_hidden(bool show);
    void set_filter(int index);
    void set_search(const std::string& text);
    void set_sort(SortColumn column, bool ascending);

    const std::vector<FileEntry>& entries() const { return shown_; }   // filtered + sorted
    std::vector<std::filesystem::path> selected_paths() const;
    bool can_accept() const;                               // a valid choice for the mode
    std::vector<std::filesystem::path> accept_paths() const;   // what "Open" returns
    const std::string& error() const { return error_; }
    ListView& list() { return *list_; }
    FileBrowserMode mode() const { return opts_.mode; }

    Signal<const std::filesystem::path&> directory_changed;
    Signal<> selection_changed;
    Signal<const std::vector<std::filesystem::path>&> files_activated;   // double-click / Enter on files

    bool on_key(const KeyEvent& e) override;

    // Column geometry shared by the header and the rows.
    struct Columns {
        float icon_x, name_x, name_w, size_right, date_x, date_w;
        static Columns for_width(float width);
    };

private:
    class Model;
    class Header;
    void load();
    void apply_view();   // filter + sort + push to the list
    void update_chrome();
    void update_status();
    void activate(size_t row);

    FileBrowserOptions opts_;
    std::filesystem::path dir_;
    std::vector<FileEntry> all_, shown_;
    std::vector<std::filesystem::path> history_;
    size_t history_index_ = 0;
    std::string search_, error_;
    int filter_ = 0;
    SortColumn sort_ = SortColumn::Name;
    bool ascending_ = true;

    std::unique_ptr<Model> model_;
    Button *back_, *forward_, *up_;
    TextField* path_;
    TextField* search_field_;
    ToggleButton* hidden_;
    ListBox* places_;
    std::vector<std::filesystem::path> place_paths_;
    Header* header_;
    ScrollView* scroll_;
    ListView* list_;
    Label* status_;
    Dropdown* filter_box_ = nullptr;
};

// Modal file dialog (in-window): a Dialog around a FileBrowser. Accepting emits `accepted`
// (with the chosen paths) and closes the dialog.
class FileDialog : public Dialog {
public:
    FileDialog(std::string title, FileBrowserOptions options);
    FileBrowser& browser() { return *browser_; }

    Signal<const std::vector<std::filesystem::path>&> accepted;
    bool was_accepted() const { return accepted_; }
    void accept();

    // Opens a dialog centred in the host's window. `on_accept` gets the chosen paths;
    // `on_cancel` runs if it was closed without choosing.
    static FileDialog& open(WidgetHost& host, std::string title, FileBrowserOptions options,
                            std::function<void(const std::vector<std::filesystem::path>&)> on_accept,
                            std::function<void()> on_cancel = {});

private:
    FileBrowser* browser_;
    Button* accept_button_;
    bool accepted_ = false;
};

} // namespace uitk
