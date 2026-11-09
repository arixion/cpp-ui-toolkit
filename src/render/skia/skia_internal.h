// Internal Skia glue shared by the CPU and GPU backends.
#pragma once
#include <memory>
#include <string>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/Paragraph.h"
#include "modules/skunicode/include/SkUnicode.h"

#include "uitk/render/canvas.h"
#include "uitk/render/renderer.h"
#include "uitk/platform/window.h"

namespace uitk {

struct Path::Impl { SkPathBuilder builder; };

namespace skglue {

inline SkCanvas* sk(void* c) { return static_cast<SkCanvas*>(c); }
inline SkRect    to_sk(const Rect& r) { return SkRect::MakeXYWH(r.x, r.y, r.width, r.height); }
inline SkColor   to_sk(Color c) { return SkColorSetARGB(c.a, c.r, c.g, c.b); }

sk_sp<SkFontMgr>                          font_manager();
sk_sp<::skia::textlayout::FontCollection>   font_collection();
sk_sp<SkUnicode>                          unicode();

// Raster backend pieces, reused by the GPU backend as its fallback.
std::unique_ptr<WindowRenderer>    make_raster_window_renderer(Window& w);
std::unique_ptr<OffscreenRenderer> make_raster_offscreen_renderer(Size logical, float scale);
bool write_png(SkSurface* surface, const std::string& path);

} // namespace skglue
} // namespace uitk
