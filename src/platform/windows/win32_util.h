// Small Win32 helpers shared by the Windows platform code. Include after <windows.h>.
#pragma once
#include <string>
#include <string_view>

#include "uitk/platform/window.h"

namespace uitk::win32 {

inline std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring out((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), out.data(), n);
    return out;
}

inline std::string narrow(std::wstring_view utf16) {
    if (utf16.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), (int)utf16.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), (int)utf16.size(), out.data(), n, nullptr, nullptr);
    return out;
}

// System cursor resource for LoadCursorW(nullptr, ...). Numeric IDs, so this works whether or
// not UNICODE is defined (the IDC_* macros change type with it).
inline const wchar_t* cursor_resource(MouseCursor c) {
    int id = 32512;                                        // IDC_ARROW
    switch (c) {
        case MouseCursor::Text:    id = 32513; break;      // IDC_IBEAM
        case MouseCursor::Pointer: id = 32649; break;      // IDC_HAND
        case MouseCursor::Move:    id = 32646; break;      // IDC_SIZEALL
        case MouseCursor::ResizeN: case MouseCursor::ResizeS:   id = 32645; break;   // IDC_SIZENS
        case MouseCursor::ResizeE: case MouseCursor::ResizeW:   id = 32644; break;   // IDC_SIZEWE
        case MouseCursor::ResizeNE: case MouseCursor::ResizeSW: id = 32643; break;   // IDC_SIZENESW
        case MouseCursor::ResizeNW: case MouseCursor::ResizeSE: id = 32642; break;   // IDC_SIZENWSE
        default: break;
    }
    return MAKEINTRESOURCEW(id);
}

} // namespace uitk::win32
