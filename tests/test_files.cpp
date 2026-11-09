// File browser, file dialog, list view and file upload tests on a temporary folder.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "uitk/core/format.h"
#include "uitk/widgets/file_browser.h"
#include "uitk/widgets/file_upload.h"
#include "uitk/widgets/window_shell.h"

using namespace uitk;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

namespace {
fs::path g_root;
void make_file(const fs::path& p, size_t bytes) { std::ofstream(p) << std::string(bytes, 'x'); }
void make_fixture() {
    g_root = fs::temp_directory_path() / ("uitk_files_test_" + std::to_string(::getpid()));
    fs::remove_all(g_root);
    fs::create_directories(g_root / "alpha");
    fs::create_directories(g_root / "beta" / "inner");
    make_file(g_root / "file2.txt", 200);
    make_file(g_root / "file10.txt", 100);
    make_file(g_root / "image.png", 5000);
    make_file(g_root / "big.bin", 90000);
    make_file(g_root / ".hidden", 10);
    make_file(g_root / "beta" / "in_beta.md", 50);
}
std::vector<std::string> names(const FileBrowser& b) {
    std::vector<std::string> out;
    for (auto& e : b.entries()) out.push_back(e.name);
    return out;
}
KeyEvent key_ev(uint32_t sym, uint32_t mods = 0) { KeyEvent k; k.keysym = sym; k.mods = mods; k.pressed = true; return k; }
PointerEvent ptr(PointerEvent::Type t, Point p, uint32_t mods = 0, int clicks = 1) {
    PointerEvent e; e.type = t; e.pos = p; e.button = PointerButton::Left; e.mods = mods; e.click_count = clicks; return e;
}
}

static void test_helpers() {
    FileFilter img{"Images", {"*.png", "*.JPG"}};
    CHECK(img.matches("a.png") && img.matches("B.jpg") && !img.matches("a.txt"));
    FileFilter all{"all", {"*"}}, q{"q", {"file?.txt"}};
    CHECK(all.matches("anything"));
    CHECK(q.matches("file2.txt") && !q.matches("file10.txt"));
    CHECK(format_bytes(999) == "999 bytes" && format_bytes(1) == "1 byte");
    CHECK(format_bytes(4100) == "4.1 kB" && format_bytes(66000000) == "66 MB");
    auto now = std::chrono::system_clock::now();
    CHECK(format_date(now - std::chrono::hours(24), now) == "Yesterday");
}

static void test_browser() {
    WindowShell shell(nullptr, "t");
    FileBrowserOptions o;
    o.mode = FileBrowserMode::OpenFiles;
    o.start_directory = g_root;
    o.filters = {{"All", {"*"}}, {"Text", {"*.txt"}}};
    FileBrowser& b = shell.content().add<FileBrowser>(o);
    b.set_expand(true, true);
    shell.host().layout({1000, 700});

    // Folders first, then natural order; hidden files hidden.
    CHECK((names(b) == std::vector<std::string>{"alpha", "beta", "big.bin", "file2.txt", "file10.txt", "image.png"}));
    b.set_show_hidden(true);
    CHECK(names(b).size() == 7);
    b.set_show_hidden(false);
    b.set_sort(SortColumn::Size, false);   // largest first; folders still first
    CHECK(names(b)[2] == "big.bin" && names(b).back() == "file10.txt");
    b.set_sort(SortColumn::Name, true);
    b.set_search("FILE");
    CHECK((names(b) == std::vector<std::string>{"file2.txt", "file10.txt"}));
    b.set_search("");
    b.set_filter(1);
    CHECK((names(b) == std::vector<std::string>{"alpha", "beta", "file2.txt", "file10.txt"}));   // folders stay
    b.set_filter(0);

    // Navigation + history.
    std::vector<fs::path> dirs;
    b.directory_changed.connect([&](const fs::path& d) { dirs.push_back(d); });
    CHECK(b.navigate(g_root / "beta"));
    CHECK(b.directory() == fs::weakly_canonical(g_root / "beta"));
    b.go_back();
    CHECK(b.directory() == fs::weakly_canonical(g_root));
    b.go_forward();
    CHECK(b.directory() == fs::weakly_canonical(g_root / "beta"));
    b.go_up();   // the folder we came from is selected
    CHECK(b.directory() == fs::weakly_canonical(g_root));
    CHECK(b.selected_paths().size() == 1 && b.selected_paths()[0].filename() == "beta");
    CHECK(!b.navigate(g_root / "does-not-exist") && !b.error().empty());
    CHECK(b.directory() == fs::weakly_canonical(g_root));   // stayed put
    CHECK(dirs.size() == 4);

    // Selection rules: folders alone cannot be "opened" as files.
    CHECK(!b.can_accept());
    std::vector<fs::path> activated;
    b.files_activated.connect([&](const std::vector<fs::path>& p) { activated = p; });
    ListView& list = b.list();
    list.grab_focus();
    list.select_only(3);   // file2.txt
    CHECK(b.can_accept() && b.accept_paths().size() == 1);
    shell.handle_event(key_ev(key::Down, MOD_SHIFT));   // extend to file10.txt
    CHECK(list.selection().size() == 2);
    shell.handle_event(key_ev(key::Return));
    CHECK(activated.size() == 2);
    // Enter on a folder opens it.
    list.select_only(0);
    shell.handle_event(key_ev(key::Return));
    CHECK(b.directory().filename() == "alpha");
    shell.handle_event(key_ev(key::Up, MOD_ALT));
    CHECK(b.directory() == fs::weakly_canonical(g_root));

    // Ctrl+click toggles, Shift+click ranges (pointer events on the list).
    shell.host().layout({1001, 700});
    float h = list.row_height();
    auto row_point = [&](int i) { return list.to_window({40, i * h + h / 2}); };
    shell.handle_event(ptr(PointerEvent::Type::Move, row_point(2)));
    shell.handle_event(ptr(PointerEvent::Type::Down, row_point(2)));
    shell.handle_event(ptr(PointerEvent::Type::Up, row_point(2)));
    shell.handle_event(ptr(PointerEvent::Type::Down, row_point(5), MOD_SHIFT));
    shell.handle_event(ptr(PointerEvent::Type::Up, row_point(5), MOD_SHIFT));
    CHECK(list.selection().size() == 4);
    shell.handle_event(ptr(PointerEvent::Type::Down, row_point(3), MOD_CTRL));
    shell.handle_event(ptr(PointerEvent::Type::Up, row_point(3), MOD_CTRL));
    CHECK(list.selection().size() == 3 && !list.is_selected(3));

    // Type-ahead jumps to "image.png".
    shell.handle_event(TextEvent{"i"});
    CHECK(list.current() == 5);
}

static void test_folder_mode_and_dialog() {
    WindowShell shell(nullptr, "t");
    shell.host().layout({1000, 700});
    FileBrowserOptions o;
    o.mode = FileBrowserMode::SelectFolder;
    o.start_directory = g_root;
    std::vector<fs::path> chosen;
    bool cancelled = false;
    FileDialog& d = FileDialog::open(shell.host(), "Pick", o, [&](const std::vector<fs::path>& p) { chosen = p; },
                                     [&] { cancelled = true; });
    shell.host().layout({1000, 700});
    CHECK(shell.host().has_popup());
    // Files are not selectable in folder mode; with nothing selected the current folder is used.
    CHECK(d.browser().can_accept());
    CHECK(d.browser().accept_paths().front() == fs::weakly_canonical(g_root));
    d.browser().list().select_only(2);   // big.bin: refused
    CHECK(d.browser().list().selection().empty());
    // Modal: a click outside is blocked and does not close the dialog.
    shell.handle_event(ptr(PointerEvent::Type::Down, {3, 3}));
    shell.handle_event(ptr(PointerEvent::Type::Up, {3, 3}));
    CHECK(shell.host().has_popup());
    d.browser().list().select_only(1);   // beta
    d.accept();
    CHECK(!shell.host().has_popup() && chosen.size() == 1 && chosen[0].filename() == "beta" && !cancelled);

    FileDialog::open(shell.host(), "Pick", o, [&](const std::vector<fs::path>&) {}, [&] { cancelled = true; });
    shell.host().layout({1000, 700});
    shell.handle_event(key_ev(key::Escape));
    CHECK(!shell.host().has_popup() && cancelled);
}

static void test_upload() {
    WindowShell shell(nullptr, "t");
    FileUpload::Options o;
    o.max_files = 3;
    o.max_file_size = 10000;
    o.filters = {{"Text", {"*.txt", "*.png"}}};
    FileUpload& up = shell.content().add<FileUpload>(o);
    shell.host().layout({800, 600});
    int changes = 0;
    up.files_changed.connect([&](const std::vector<fs::path>&) { ++changes; });
    size_t n = up.add_files({g_root / "file2.txt", g_root / "big.bin", g_root / "image.png", g_root / "file2.txt"});
    CHECK(n == 2 && up.files().size() == 2 && changes == 1);   // big.bin: wrong type; duplicate ignored
    CHECK(up.add_files({g_root / "file10.txt", g_root / "beta" / "in_beta.md"}) == 1);   // .md refused
    CHECK(up.add_files({g_root / "file2.txt"}) == 0);
    up.set_status(1, FileUpload::Status::Uploading);
    up.set_progress(1, 0.5f);
    up.set_status(2, FileUpload::Status::Done);
    up.remove_file(0);
    CHECK(up.files().size() == 2 && up.status(0) == FileUpload::Status::Uploading && up.status(1) == FileUpload::Status::Done);
    CHECK(up.add_files({g_root / "file2.txt", g_root / "file2.txt"}) == 1);
    CHECK(up.add_files({g_root / "image.png"}) == 0);   // already listed
    up.clear();
    CHECK(up.files().empty());

    FileUpload::Options big;
    big.max_file_size = 1000;
    FileUpload& up2 = shell.content().add<FileUpload>(big);
    CHECK(up2.add_files({g_root / "big.bin"}) == 0);   // too large
}

int main() {
    make_fixture();
    test_helpers();
    test_browser();
    test_folder_mode_and_dialog();
    test_upload();
    fs::remove_all(g_root);
    if (failures) { fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    printf("test_files: all checks passed\n");
    return 0;
}
