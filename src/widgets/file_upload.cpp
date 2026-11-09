#include "uitk/widgets/file_upload.h"

#include <algorithm>
#include <cmath>

#include "uitk/core/application.h"
#include "uitk/core/format.h"
#include "uitk/core/paths.h"
#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/icon.h"
#include "uitk/widgets/icons.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/range.h"
#include "uitk/widgets/widget_host.h"

namespace fs = std::filesystem;

namespace uitk {

// ---------------------------------------------------------------------------- drop zone

// Dashed card; the whole card is clickable (opens the dialog) and has hover feedback.
class FileUpload::DropZone : public Box {
public:
    explicit DropZone(FileUpload& owner) : Box(Orientation::Vertical, 6.f), owner_(owner) {
        set_padding(Insets{16, 20, 16, 20});
        set_align(Align::Fill, Align::Start);
        Icon& ic = add<Icon>(icons::symbolic("document-send"), 32);
        ic.set_token(ColorToken::Accent);
        ic.set_align(Align::Center, Align::Start);
        title_ = &add<Label>(owner.opts_.title, TextRole::Button);
        title_->set_align(Align::Center, Align::Start);
        hint_ = &add<Label>("", TextRole::Dim);
        hint_->set_align(Align::Center, Align::Start);
        add<Widget>().set_min_size({1, 4});
        Button& browse = add<Button>(owner.opts_.multiple ? "Browse files…" : "Browse…", ButtonStyle::Primary);
        browse.set_align(Align::Center, Align::Start);
        browse.clicked.connect([this] { owner_.browse(); });
    }
    void set_hint(std::string h) { hint_->set_text(std::move(h)); }

    bool accepts_pointer() const override { return true; }
    bool on_pointer(const PointerEvent& e) override {
        if (e.type == PointerEvent::Type::Up && e.button == PointerButton::Left && local_rect().contains(e.pos)) owner_.browse();
        return e.type == PointerEvent::Type::Down || e.type == PointerEvent::Type::Up;
    }
    void paint(PaintContext& ctx) override {
        const Theme& t = ctx.theme;
        StateLayer::Sample s = layer_.sample(ctx.clock);
        Rect r = local_rect().inset(0.5f, 0.5f);
        ctx.canvas.draw_rrect(r, t.card_radius, Paint::fill(StateLayer::surface(t.input_bg, t.foreground, s, t)));
        Color border = lerp(t.control_border, t.accent, s.hover);
        ctx.canvas.draw_rrect(r, t.card_radius, Paint::dashed(border, 1.5f, 6, 4));
    }

protected:
    void on_state_changed(uint32_t) override { layer_.update(state(), theme()); }

private:
    FileUpload& owner_;
    Label* title_;
    Label* hint_;
    StateLayer layer_;
};

// ---------------------------------------------------------------------------- FileUpload

FileUpload::FileUpload(Options options) : Box(Orientation::Vertical, 10.f), opts_(std::move(options)) {
    set_align(Align::Fill, Align::Start);
    zone_ = &add<DropZone>(*this);
    zone_->set_hint(opts_.hint.empty() ? default_hint() : opts_.hint);
    notice_ = &add<Label>("", TextRole::Dim);
    notice_->set_wrap(true);
    notice_->set_color_token(ColorToken::Error);
    notice_->set_visible(false);
    list_ = &add<Box>(Orientation::Vertical, 4.f);
}

std::string FileUpload::default_hint() const {
    std::string types = "Any file";
    if (!opts_.filters.empty()) {
        types.clear();
        for (auto& f : opts_.filters) types += (types.empty() ? "" : ", ") + f.name;
    }
    std::string h = types;
    if (opts_.max_file_size) h += " · up to " + format_bytes(opts_.max_file_size) + " each";
    if (opts_.max_files) h += " · max " + std::to_string(opts_.max_files) + (opts_.max_files == 1 ? " file" : " files");
    return h;
}

void FileUpload::browse() {
    WidgetHost* h = host();
    if (!h) return;
    FileBrowserOptions fo;
    fo.mode = opts_.multiple ? FileBrowserMode::OpenFiles : FileBrowserMode::OpenFile;
    fo.start_directory = last_dir_;
    fo.filters = opts_.filters;
    if (!fo.filters.empty()) fo.filters.push_back({"All files", {"*"}});
    FileDialog::open(*h, opts_.multiple ? "Choose files to upload" : "Choose a file to upload", fo,
                     [this](const std::vector<fs::path>& paths) {
                         if (!paths.empty()) last_dir_ = paths.front().parent_path();
                         add_files(paths);
                     });
}

size_t FileUpload::add_files(const std::vector<fs::path>& paths) {
    std::vector<std::string> problems;
    size_t added = 0;
    if (!opts_.multiple && !paths.empty()) { files_.clear(); }
    for (const fs::path& p : paths) {
        std::string name = paths::to_utf8(p.filename());
        if (std::find(files_.begin(), files_.end(), p) != files_.end()) continue;   // already listed
        if (opts_.max_files && files_.size() >= opts_.max_files) {
            problems.push_back("Only " + std::to_string(opts_.max_files) + (opts_.max_files == 1 ? " file" : " files") + " can be added.");
            break;
        }
        bool type_ok = opts_.filters.empty() ||
                       std::any_of(opts_.filters.begin(), opts_.filters.end(), [&](const FileFilter& f) { return f.matches(name); });
        if (!type_ok) { problems.push_back("“" + name + "” is not an accepted file type."); continue; }
        std::error_code ec;
        uint64_t size = fs::file_size(p, ec);
        if (ec) { problems.push_back("“" + name + "” cannot be read."); continue; }
        if (opts_.max_file_size && size > opts_.max_file_size) {
            problems.push_back("“" + name + "” is larger than " + format_bytes(opts_.max_file_size) + ".");
            continue;
        }
        files_.push_back(p);
        ++added;
    }
    std::string text;
    for (auto& m : problems) text += (text.empty() ? "" : "\n") + m;
    notice_->set_text(text);
    notice_->set_visible(!text.empty());
    rebuild_rows();
    if (added) files_changed.emit(files_);
    return added;
}

void FileUpload::remove_file(size_t index) {
    if (index >= files_.size()) return;
    files_.erase(files_.begin() + (long)index);
    rebuild_rows();
    files_changed.emit(files_);
}

void FileUpload::clear() {
    if (files_.empty()) return;
    files_.clear();
    rebuild_rows();
    files_changed.emit(files_);
}

void FileUpload::rebuild_rows() {
    list_->clear_children();
    std::vector<Row> fresh;
    for (size_t i = 0; i < files_.size(); ++i) {
        const fs::path& p = files_[i];
        Row r;
        r.path = p;
        Frame& card = list_->add<Frame>(FrameStyle::Card, Orientation::Horizontal, 10.f);
        card.set_padding(Insets{10, 6, 6, 6});
        r.widget = &card;
        Icon& ic = card.add<Icon>(icons::for_file(p, false));
        ic.set_token(ColorToken::ForegroundDim);
        Box& text = card.add<Box>(Orientation::Vertical, 2.f);
        text.set_expand(true, false);
        Label& name = text.add<Label>(paths::to_utf8(p.filename()));
        name.set_ellipsize(true);
        std::error_code ec;
        r.size = fs::file_size(p, ec);
        r.detail = &text.add<Label>("", TextRole::Caption);
        r.progress = &card.add<ProgressBar>(0.f);
        r.progress->set_fixed_size({120, 0});
        r.state_icon = &card.add<Icon>(Image(), 16);
        Button& remove = card.add<Button>(glyph(Glyph::Close), ButtonStyle::Flat);
        remove.set_tooltip("Remove");
        remove.clicked.connect([this, i] {
            // Defer: removing destroys this button while its click is being handled.
            if (Application* app = Application::current()) app->post([this, i] { remove_file(i); });
            else remove_file(i);
        });
        // Files that stay in the list keep their upload status.
        for (const Row& old : rows_) if (old.path == p) r.status = old.status;
        fresh.push_back(r);
    }
    rows_ = std::move(fresh);
    for (size_t i = 0; i < rows_.size(); ++i) update_row(i);
}

void FileUpload::update_row(size_t i, const std::string& message) {
    Row& r = rows_[i];
    std::string detail = format_bytes(r.size);
    switch (r.status) {
        case Status::Pending:   detail += " · Ready"; break;
        case Status::Uploading: detail += " · Uploading…"; break;
        case Status::Done:      detail += " · Uploaded"; break;
        case Status::Failed:    detail += " · " + (message.empty() ? std::string("Failed") : message); break;
    }
    r.detail->set_text(detail);
    r.progress->set_visible(r.status == Status::Uploading);
    r.state_icon->set_visible(r.status == Status::Done || r.status == Status::Failed);
    r.state_icon->set_image(glyph(r.status == Status::Failed ? Glyph::Error : Glyph::Check));
    r.state_icon->set_token(r.status == Status::Failed ? ColorToken::Error : ColorToken::Accent);
}

void FileUpload::set_status(size_t index, Status status, std::string message) {
    if (index >= rows_.size()) return;
    rows_[index].status = status;
    update_row(index, message);
}

void FileUpload::set_progress(size_t index, float fraction) {
    if (index >= rows_.size()) return;
    rows_[index].progress->set_fraction(fraction);
}

} // namespace uitk
