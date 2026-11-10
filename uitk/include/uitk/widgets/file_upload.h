// FileUpload: pick files for upload with the in-app FileDialog (no native dialog).
//
//   FileUpload::Options o;
//   o.filters = {{"Images", {"*.png", "*.jpg"}}};
//   o.max_files = 5;  o.max_file_size = 10'000'000;
//   auto& up = box.add<FileUpload>(o);
//   up.files_changed.connect([&](auto& files) { ... });
//   // while transferring (the transfer itself is the application's job):
//   up.set_status(i, FileUpload::Status::Uploading);  up.set_progress(i, 0.4f);
//
// Built from Frame, Icon, Label, Button, ProgressBar and FileDialog.
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/file_browser.h"

namespace uitk {

class ProgressBar;
class Icon;

struct FileUploadOptions {
    std::string title = "Upload files";
    std::string hint;                     // empty: generated from the limits
    bool multiple = true;
    std::vector<FileFilter> filters;      // accepted types (empty: any)
    size_t max_files = 0;                 // 0: unlimited
    uint64_t max_file_size = 0;           // bytes; 0: unlimited
};

class FileUpload : public Box {
public:
    enum class Status { Pending, Uploading, Done, Failed };
    using Options = FileUploadOptions;

    explicit FileUpload(Options options = Options{});

    const std::vector<std::filesystem::path>& files() const { return files_; }
    // Adds files, rejecting duplicates and anything over the limits (returns how many were added;
    // reasons for rejections are shown under the drop zone).
    size_t add_files(const std::vector<std::filesystem::path>& paths);
    void remove_file(size_t index);
    void clear();
    void browse();   // opens the file dialog

    // Upload feedback per file.
    void set_status(size_t index, Status status, std::string message = {});
    void set_progress(size_t index, float fraction);
    Status status(size_t index) const { return rows_[index].status; }

    Signal<const std::vector<std::filesystem::path>&> files_changed;

private:
    class DropZone;
    struct Row {
        std::filesystem::path path;
        Box* widget = nullptr;
        Label* detail = nullptr;
        ProgressBar* progress = nullptr;
        Icon* state_icon = nullptr;
        uint64_t size = 0;
        Status status = Status::Pending;
    };
    void rebuild_rows();
    void update_row(size_t index, const std::string& message = {});
    std::string default_hint() const;

    Options opts_;
    std::vector<std::filesystem::path> files_;
    std::vector<Row> rows_;
    DropZone* zone_;
    Label* notice_;
    Box* list_;
    std::filesystem::path last_dir_;
};

} // namespace uitk
