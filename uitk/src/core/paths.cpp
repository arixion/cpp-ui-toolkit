#include "uitk/core/paths.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdlib>

namespace uitk::paths {

std::string to_utf8(const stdfs::path& p) {
    std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

stdfs::path from_utf8(std::string_view utf8) {
    return stdfs::path(std::u8string(utf8.begin(), utf8.end()));
}

stdfs::path home_directory() {
#ifdef _WIN32
    if (const wchar_t* h = _wgetenv(L"USERPROFILE"); h && *h) return stdfs::path(h);
    const wchar_t* drive = _wgetenv(L"HOMEDRIVE");
    const wchar_t* path = _wgetenv(L"HOMEPATH");
    if (drive && path) return stdfs::path(std::wstring(drive) + path);
    return stdfs::path(L"C:\\");
#else
    const char* h = getenv("HOME");
    return h && *h ? stdfs::path(h) : stdfs::path("/");
#endif
}

std::vector<stdfs::path> roots() {
#ifdef _WIN32
    std::vector<stdfs::path> out;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i))) continue;
        wchar_t root[] = {wchar_t(L'A' + i), L':', L'\\', 0};
        UINT type = GetDriveTypeW(root);
        if (type == DRIVE_NO_ROOT_DIR || type == DRIVE_UNKNOWN) continue;
        out.emplace_back(root);
    }
    return out;
#else
    return {stdfs::path("/")};
#endif
}

bool is_hidden(const stdfs::path& p) {
    stdfs::path name = p.filename();
    if (!name.empty() && name.native()[0] == '.' && name != "." && name != "..") return true;
#ifdef _WIN32
    DWORD a = GetFileAttributesW(p.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_HIDDEN)) return true;
#endif
    return false;
}

stdfs::path expand_user(std::string_view text) {
    if (text == "~") return home_directory();
    if (text.size() >= 2 && text[0] == '~' && (text[1] == '/' || text[1] == '\\'))
        return home_directory() / from_utf8(text.substr(2));
    return from_utf8(text);
}

} // namespace uitk::paths
