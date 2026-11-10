// "files" example: the FileUpload widget and an embedded FileBrowser (no native dialogs).
//
//   files_cpu                         run the app
//   files_cpu --dir PATH              start browsing in PATH
//   files_cpu --snapshot out.png [--dark] [--dialog] [--uploads] [--size WxH] [--dir PATH]
//                                     render offscreen (same UI code) and exit
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#include "uitk/core/paths.h"
#include "uitk/core/application.h"
#include "uitk/render/renderer.h"
#include "uitk/widgets/application_window.h"
#include "uitk/widgets/file_browser.h"
#include "uitk/widgets/file_upload.h"
#include "uitk/widgets/icons.h"
#include "uitk/widgets/status_bar.h"

using namespace uitk;
namespace fs = std::filesystem;

namespace {

struct Ui {
    FileUpload* upload = nullptr;
    FileBrowser* browser = nullptr;
    Button* start = nullptr;
    Button* clear = nullptr;
    Button* status = nullptr;
};

Ui build_ui(WindowShell& shell, const fs::path& start_dir) {
    Ui ui;
    Box& body = shell.content().add<Box>(Orientation::Horizontal, 20.f);
    body.set_padding(Insets{20, 16, 20, 16});
    body.set_expand(true, true);

    // ---- left: upload ----
    Box& left = body.add<Box>(Orientation::Vertical, 12.f);
    left.set_fixed_size({400, 0});
    left.add<Label>("Upload", TextRole::Heading).set_align(Align::Start, Align::Center);
    Label& intro = left.add<Label>("Files are chosen with the toolkit's own file browser, not the "
                                   "platform dialog. Limits: 8 files, 50 MB each.", TextRole::Dim);
    intro.set_wrap(true);
    FileUpload::Options o;
    o.title = "Choose files to upload";
    o.max_files = 8;
    o.max_file_size = 50'000'000;
    ui.upload = &left.add<FileUpload>(o);
    Box& actions = left.add<Box>(Orientation::Horizontal, 8.f);
    ui.start = &actions.add<Button>("Start upload", ButtonStyle::Primary);
    ui.clear = &actions.add<Button>("Clear");
    ui.start->set_enabled(false);
    ui.clear->set_enabled(false);
    ui.upload->files_changed.connect([ui](const std::vector<fs::path>& f) {
        ui.start->set_enabled(!f.empty());
        ui.clear->set_enabled(!f.empty());
    });
    ui.clear->clicked.connect([up = ui.upload] { up->clear(); });

    // ---- right: embedded browser ----
    Box& right = body.add<Box>(Orientation::Vertical, 8.f);
    right.set_expand(true, true);
    right.add<Label>("Embedded file browser", TextRole::Chrome).set_align(Align::Start, Align::Center);
    Frame& frame = right.add<Frame>(FrameStyle::Card, Orientation::Vertical, 0.f);
    frame.set_expand(true, true);
    FileBrowserOptions bo;
    bo.mode = FileBrowserMode::OpenFiles;
    bo.start_directory = start_dir;
    bo.filters = {{"All files", {"*"}}, {"Images", {"*.png", "*.jpg", "*.jpeg", "*.gif", "*.svg", "*.webp"}},
                  {"Documents", {"*.pdf", "*.md", "*.txt", "*.odt", "*.docx"}}, {"Source code", {"*.cpp", "*.h", "*.c", "*.py", "*.rs", "*.ts", "*.js"}}};
    ui.browser = &frame.add<FileBrowser>(bo);
    Button& add_sel = right.add<Button>("Add selected to upload");
    add_sel.set_enabled(false);
    add_sel.set_icon(icons::symbolic("list-add"));
    FileBrowser* br = ui.browser;
    FileUpload* up = ui.upload;
    Button* add_btn = &add_sel;
    br->selection_changed.connect([br, add_btn] { add_btn->set_enabled(br->can_accept()); });
    add_sel.clicked.connect([br, up] { up->add_files(br->accept_paths()); });
    br->files_activated.connect([up](const std::vector<fs::path>& p) { up->add_files(p); });

    // ---- status bar ----
    StatusBar& bar = shell.content().add<StatusBar>();
    ui.status = &bar.add_start("", icons::symbolic("folder"));
    bar.add_end("Double-click a file to add it · Alt+↑ parent · Ctrl+L path");
    br->directory_changed.connect([s = ui.status](const fs::path& d) { s->set_label(paths::to_utf8(d)); });
    ui.status->set_label(paths::to_utf8(br->directory()));
    return ui;
}

// Simulated transfer so the per-file progress / status UI can be seen.
void simulate_upload(Application& app, FileUpload& up) {
    auto step = std::make_shared<std::vector<float>>(up.files().size(), 0.f);
    for (size_t i = 0; i < up.files().size(); ++i) up.set_status(i, FileUpload::Status::Uploading);
    auto timer = std::make_shared<TimerId>(0);
    *timer = app.add_timer(std::chrono::milliseconds(120), [&app, &up, step, timer] {
        bool any = false;
        for (size_t i = 0; i < step->size() && i < up.files().size(); ++i) {
            float& f = (*step)[i];
            if (f >= 1) continue;
            f = std::min(1.f, f + 0.04f + 0.03f * (float)(i % 3));
            up.set_progress(i, f);
            if (f >= 1) {
                // Pretend the 3rd file is rejected by the server, to show the error state.
                if (i == 2) up.set_status(i, FileUpload::Status::Failed, "Server rejected the file");
                else up.set_status(i, FileUpload::Status::Done);
            }
            any = true;
        }
        if (!any) app.cancel_timer(*timer);
    }, true);
}

int snapshot(const char* out, bool dark, bool dialog, bool uploads, Size size, const fs::path& dir) {
    WindowShell shell(nullptr, "uitk files (snapshot)");
    shell.set_theme(dark ? Theme::vscode_dark() : Theme::vscode_light(), false);
    Ui ui = build_ui(shell, dir);
    if (uploads) {
        std::vector<fs::path> files;
        std::error_code ec;
        for (auto& e : fs::directory_iterator(dir, ec))
            if (e.is_regular_file() && files.size() < 4) files.push_back(e.path());
        ui.upload->add_files(files);
        if (files.size() > 0) { ui.upload->set_status(0, FileUpload::Status::Done); }
        if (files.size() > 1) { ui.upload->set_status(1, FileUpload::Status::Uploading); ui.upload->set_progress(1, 0.6f); }
        if (files.size() > 2) { ui.upload->set_status(2, FileUpload::Status::Failed, "Server rejected the file"); }
        if (ui.browser->entries().size() > 2) { ui.browser->list().select_only(1); ui.browser->list().grab_focus(); }
    }
    shell.host().layout(size);
    if (dialog) {
        FileBrowserOptions fo;
        fo.mode = FileBrowserMode::OpenFiles;
        fo.start_directory = dir;
        fo.filters = {{"All files", {"*"}}, {"Images", {"*.png", "*.jpg", "*.svg"}}};
        FileDialog& d = FileDialog::open(shell.host(), "Choose files to upload", fo, nullptr);
        if (d.browser().entries().size() > 3) {
            d.browser().list().select_only(2);
            KeyEvent k; k.keysym = key::Down; k.mods = mod::Shift; k.pressed = true;
            shell.handle_event(k);   // extend the selection with Shift+Down
        }
        shell.host().layout(size);
    }
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
    bool dark = false, dialog = false, uploads = false;
    Size size{1180, 720};
    fs::path dir;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--snapshot") && i + 1 < argc) snap = argv[++i];
        else if (!strcmp(argv[i], "--dark")) dark = true;
        else if (!strcmp(argv[i], "--dialog")) dialog = true;
        else if (!strcmp(argv[i], "--uploads")) uploads = true;
        else if (!strcmp(argv[i], "--dir") && i + 1 < argc) dir = paths::from_utf8(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            int w, h; if (sscanf(argv[++i], "%dx%d", &w, &h) == 2) size = {(float)w, (float)h};
        }
    }
    if (snap) return snapshot(snap, dark, dialog, uploads, size, dir);

    Application app;
    WindowDesc desc;
    desc.title = "uitk files";
    desc.app_id = "uitk-files";
    desc.size = size;
    ApplicationWindow win(app, desc);
    win.set_theme(Theme::vscode_dark(), false);
    Ui ui = build_ui(win.shell(), dir);
    ui.start->clicked.connect([&app, up = ui.upload] { simulate_upload(app, *up); });
    return app.run();
}
