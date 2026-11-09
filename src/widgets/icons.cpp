#include "uitk/widgets/icons.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <unordered_map>

#include "uitk/core/paths.h"

#ifndef UITK_DEFAULT_ICON_DIR
#define UITK_DEFAULT_ICON_DIR "assets/icons/adwaita/symbolic"
#endif

namespace uitk::icons {

namespace {
std::string& path_ref() {
    static std::string p = UITK_DEFAULT_ICON_DIR;
    return p;
}
std::unordered_map<std::string, Image>& cache() {
    static std::unordered_map<std::string, Image> c;
    return c;
}
} // namespace

void set_search_path(std::string directory) {
    path_ref() = std::move(directory);
    cache().clear();
}

const std::string& search_path() { return path_ref(); }

const Image& symbolic(std::string_view name) {
    std::string key(name);
    auto it = cache().find(key);
    if (it != cache().end()) return it->second;

    static const char* categories[] = {"actions", "places", "status", "ui", "devices", "mimetypes",
                                       "categories", "emotes", "apps", "legacy"};
    Image img;
    for (const char* cat : categories) {
        std::filesystem::path p = paths::from_utf8(path_ref()) / cat / (key + "-symbolic.svg");
        if (std::filesystem::exists(p)) { img = Image::load(paths::to_utf8(p)); break; }
    }
    if (!img.valid()) fprintf(stderr, "uitk: icon '%s' not found in %s\n", key.c_str(), path_ref().c_str());
    return cache().emplace(std::move(key), std::move(img)).first->second;
}

const Image& for_file(const std::filesystem::path& path, bool is_directory) {
    if (is_directory) return symbolic("folder");
    std::string ext = paths::to_utf8(path.extension());
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    static const std::unordered_map<std::string, const char*> by_ext = {
        {".png", "image-x-generic"}, {".jpg", "image-x-generic"}, {".jpeg", "image-x-generic"},
        {".gif", "image-x-generic"}, {".webp", "image-x-generic"}, {".svg", "image-x-generic"},
        {".bmp", "image-x-generic"}, {".ico", "image-x-generic"},
        {".mp3", "audio-x-generic"}, {".wav", "audio-x-generic"}, {".flac", "audio-x-generic"},
        {".ogg", "audio-x-generic"}, {".m4a", "audio-x-generic"},
        {".mp4", "video-x-generic"}, {".mkv", "video-x-generic"}, {".webm", "video-x-generic"},
        {".mov", "video-x-generic"}, {".avi", "video-x-generic"},
        {".zip", "package-x-generic"}, {".tar", "package-x-generic"}, {".gz", "package-x-generic"},
        {".xz", "package-x-generic"}, {".7z", "package-x-generic"}, {".rar", "package-x-generic"},
        {".deb", "package-x-generic"}, {".rpm", "package-x-generic"},
        {".pdf", "x-office-document"}, {".doc", "x-office-document"}, {".docx", "x-office-document"},
        {".odt", "x-office-document"}, {".rtf", "x-office-document"},
        {".xls", "x-office-spreadsheet"}, {".xlsx", "x-office-spreadsheet"}, {".ods", "x-office-spreadsheet"},
        {".csv", "x-office-spreadsheet"},
        {".ppt", "x-office-presentation"}, {".pptx", "x-office-presentation"}, {".odp", "x-office-presentation"},
        {".ttf", "font-x-generic"}, {".otf", "font-x-generic"}, {".woff", "font-x-generic"}, {".woff2", "font-x-generic"},
        {".so", "application-x-addon"}, {".dll", "application-x-addon"},
        {".exe", "application-x-executable"}, {".appimage", "application-x-executable"}, {".sh", "application-x-executable"},
    };
    auto it = by_ext.find(ext);
    return symbolic(it != by_ext.end() ? it->second : "text-x-generic");
}

} // namespace uitk::icons
