// Thin line glyphs for UI chrome (window controls, chevrons, check marks), drawn on a 16 px
// pixel grid with 1 px strokes so they stay crisp. Single-colour: tint them like any symbolic
// icon (canvas.draw_image(img, rect, colour)). Embedded: no files needed.
#pragma once
#include "uitk/render/canvas.h"

namespace uitk {

enum class Glyph {
    Minimize, Maximize, Restore, Close,
    ChevronDown, ChevronUp, ChevronLeft, ChevronRight,
    Check, More,   // "…" (more actions)
    Info, Warning, Error,   // outlined status icons (notifications, problems)
    Plus, Minus,
};

const Image& glyph(Glyph g);

} // namespace uitk
