// Platform-independent input and window events.
#pragma once
#include <cstdint>
#include <string>
#include <variant>

#include "uitk/core/geometry.h"

namespace uitk {

enum Modifier : uint32_t {
    MOD_NONE = 0, MOD_SHIFT = 1, MOD_CTRL = 2, MOD_ALT = 4, MOD_SUPER = 8,
};

enum class PointerButton { NoButton, Left, Right, Middle, Other };

struct PointerEvent {
    enum class Type { Enter, Leave, Move, Down, Up } type = Type::Move;
    Point         pos;                       // logical px, window-relative
    PointerButton button = PointerButton::NoButton;
    int           click_count = 0;           // 1 = single, 2 = double ... (Down only)
    uint32_t      mods = MOD_NONE;
    uint32_t      time_ms = 0;
};

struct ScrollEvent {
    Point    pos;
    float    dx = 0, dy = 0;                 // logical px (positive = right/down)
    uint32_t mods = MOD_NONE;
};

// Keysyms use the XKB/X11 keysym space (e.g. 'a' = 0x61, Escape = 0xff1b), see uitk::key.
struct KeyEvent {
    bool     pressed = true;
    bool     repeat  = false;
    uint32_t keysym  = 0;
    uint32_t mods    = MOD_NONE;
};

struct TextEvent   { std::string utf8; };     // committed text input
struct ResizeEvent { Size size; float scale = 1; };
struct FocusEvent  { bool focused = false; };
struct StateEvent  { bool maximized = false; };
struct CloseEvent  {};                       // user/compositor asked to close the window

using Event = std::variant<PointerEvent, ScrollEvent, KeyEvent, TextEvent,
                           ResizeEvent, FocusEvent, StateEvent, CloseEvent>;

namespace key {
constexpr uint32_t BackSpace = 0xff08, Tab = 0xff09, Return = 0xff0d, Escape = 0xff1b,
                   Delete = 0xffff, Home = 0xff50, Left = 0xff51, Up = 0xff52, Right = 0xff53,
                   Down = 0xff54, PageUp = 0xff55, PageDown = 0xff56, End = 0xff57,
                   F1 = 0xffbe, F11 = 0xffc8, F12 = 0xffc9;
// Lower-case latin letters map to their ASCII codes.
constexpr uint32_t letter(char c) { return (c >= 'A' && c <= 'Z') ? uint32_t(c - 'A' + 'a') : uint32_t(c); }
} // namespace key

} // namespace uitk
