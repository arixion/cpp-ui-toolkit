#include "uitk/widgets/glyphs.h"

namespace uitk {

namespace {
// Coordinates at .5 put 1 px strokes exactly on pixel rows/columns at 1x.
#define SVG16(body) "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 16 16'>" body "</svg>"
#define STROKE "fill='none' stroke='#000' stroke-width='1'"

const char* source(Glyph g) {
    switch (g) {
        case Glyph::Minimize:     return SVG16("<path " STROKE " d='M3 8.5h10'/>");
        case Glyph::Maximize:     return SVG16("<rect " STROKE " x='3.5' y='3.5' width='9' height='9'/>");
        case Glyph::Restore:      return SVG16("<rect " STROKE " x='3.5' y='5.5' width='7' height='7'/>"
                                               "<path " STROKE " d='M5.5 5.5v-2h7v7h-2'/>");
        case Glyph::Close:        return SVG16("<path " STROKE " d='M3.5 3.5l9 9M12.5 3.5l-9 9'/>");
        case Glyph::ChevronDown:  return SVG16("<path " STROKE " stroke-linecap='round' stroke-linejoin='round' d='M4.5 6.5l3.5 3.5 3.5-3.5'/>");
        case Glyph::ChevronUp:    return SVG16("<path " STROKE " stroke-linecap='round' stroke-linejoin='round' d='M4.5 9.5l3.5-3.5 3.5 3.5'/>");
        case Glyph::ChevronLeft:  return SVG16("<path " STROKE " stroke-linecap='round' stroke-linejoin='round' d='M9.5 4.5l-3.5 3.5 3.5 3.5'/>");
        case Glyph::ChevronRight: return SVG16("<path " STROKE " stroke-linecap='round' stroke-linejoin='round' d='M6.5 4.5l3.5 3.5-3.5 3.5'/>");
        case Glyph::Check:        return SVG16("<path " STROKE " stroke-linecap='round' stroke-linejoin='round' d='M3.5 8.5l3 3 6-7'/>");
        case Glyph::More:         return SVG16("<circle cx='3.5' cy='8.5' r='1.1'/><circle cx='8' cy='8.5' r='1.1'/>"
                                               "<circle cx='12.5' cy='8.5' r='1.1'/>");
        case Glyph::Info:         return SVG16("<circle " STROKE " cx='8' cy='8' r='6.5'/>"
                                               "<path " STROKE " d='M8 7v4.5'/><circle cx='8' cy='5' r='0.8'/>");
        case Glyph::Warning:      return SVG16("<path " STROKE " stroke-linejoin='round' d='M8 2l6.5 11.5h-13z'/>"
                                               "<path " STROKE " d='M8 6.5v3.5'/><circle cx='8' cy='12' r='0.7'/>");
        case Glyph::Error:        return SVG16("<circle " STROKE " cx='8' cy='8' r='6.5'/>"
                                               "<path " STROKE " d='M5.5 5.5l5 5M10.5 5.5l-5 5'/>");
        case Glyph::Plus:         return SVG16("<path " STROKE " d='M8 3v10M3 8.5h10'/>");
        case Glyph::Minus:        return SVG16("<path " STROKE " d='M3 8.5h10'/>");
    }
    return SVG16("");
}
#undef STROKE
#undef SVG16
} // namespace

const Image& glyph(Glyph g) {
    static Image cache[(int)Glyph::Minus + 1];
    Image& img = cache[(int)g];
    if (!img.valid()) img = Image::from_svg(source(g));
    return img;
}

} // namespace uitk
