// Drawing API. Implemented by both render backends (CPU raster / GPU), which share one engine,
// so output is identical. Coordinates are logical px; the renderer applies the window scale.
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "uitk/core/bezier.h"
#include "uitk/core/color.h"
#include "uitk/core/geometry.h"

namespace uitk {

struct Paint {
    enum class Style { Fill, Stroke };
    Color color;
    Style style = Style::Fill;
    float stroke_width = 1;
    bool  antialias = true;
    float dash_on = 0, dash_off = 0;   // dashed strokes when dash_on > 0

    static Paint fill(Color c) { return {c}; }
    static Paint stroke(Color c, float width = 1) { return {c, Style::Stroke, width}; }
    static Paint dashed(Color c, float width, float on, float off) { return {c, Style::Stroke, width, true, on, off}; }
};

class Path {
public:
    Path();
    ~Path();
    Path(const Path&);
    Path& operator=(const Path&);

    Path& move_to(Point p);
    Path& line_to(Point p);
    Path& quad_to(Point c, Point p);
    Path& cubic_to(Point c1, Point c2, Point p);
    Path& add_bezier(const CubicBezier& b);   // move_to(p0) + cubic_to(p1, p2, p3)
    Path& close();
    Path& add_rect(const Rect& r);
    Path& add_rrect(const Rect& r, float radius);
    Path& add_circle(Point c, float radius);
    Path& add_arc(const Rect& oval, float start_degrees, float sweep_degrees);   // 0° = 3 o'clock, clockwise

    struct Impl;
private:
    std::unique_ptr<Impl> impl_;
    friend class Canvas;
};

// Decoded raster image (PNG/JPEG/WebP) or vector image (SVG). Cheap to copy (shared).
class Image {
public:
    Image() = default;
    static Image load(const std::string& path);                 // by extension: .svg => vector
    static Image decode(const void* data, size_t len);          // raster formats
    static Image from_svg(std::string_view svg_source);

    bool valid() const { return impl_ != nullptr; }
    bool is_vector() const;
    Size size() const;   // intrinsic size in logical px

    struct Impl;
private:
    std::shared_ptr<Impl> impl_;
    friend class Canvas;
};

enum class TextAlign { Left, Center, Right, Justify };

struct TextStyle {
    std::vector<std::string> families = {"Adwaita Sans", "Cantarell", "Noto Sans", "DejaVu Sans", "sans-serif"};
    float     size = 14;
    int       weight = 400;      // 100..900
    bool      italic = false;
    Color     color = Color::rgb(0, 0, 0);
    TextAlign align = TextAlign::Left;
    int       max_lines = 0;     // 0 = unlimited
    bool      ellipsis = false;  // with max_lines: end the last line with "…"
};

// Shaped, laid-out paragraph (HarfBuzz shaping, ICU line breaking, font fallback).
class TextLayout {
public:
    TextLayout() = default;
    TextLayout(std::string_view utf8, const TextStyle& style, float max_width = 1e9f);

    void  layout(float max_width);   // re-wrap to a new width
    Size  size() const;              // {longest line, height}
    float max_intrinsic_width() const;

    // ---- editing support (UTF-8 byte offsets) ----
    float  caret_x(size_t byte_index) const;    // x of the caret before `byte_index` (first line)
    size_t index_at(float x, float y = 0) const;  // nearest caret position to a point
    std::vector<Rect> range_rects(size_t from, size_t to) const;   // selection highlight boxes
    const std::string& text() const;
    bool  valid() const { return impl_ != nullptr; }

    struct Impl;
private:
    std::shared_ptr<Impl> impl_;   // shaped layout, shared via a cache (colour-independent)
    Color color_;                  // applied at draw time: colour changes never reshape text
    friend class Canvas;
};

class Canvas {
public:
    // Internal: renderers construct canvases around their engine canvas.
    explicit Canvas(void* engine_canvas) : c_(engine_canvas) {}

    void save();
    void save_layer(float opacity);    // group with opacity; balance with restore()
    void restore();
    void translate(float dx, float dy);
    void scale(float sx, float sy);
    void rotate(float degrees, Point around);   // clockwise
    void clip_rect(const Rect& r);
    void clip_rrect(const Rect& r, float radius);
    void clip_rrect(const Rect& r, const CornerRadii& radii);

    void clear(Color c);
    void draw_rect(const Rect& r, const Paint& p);
    void draw_rrect(const Rect& r, float radius, const Paint& p);
    void draw_rrect(const Rect& r, const CornerRadii& radii, const Paint& p);
    void draw_circle(Point c, float radius, const Paint& p);
    void draw_line(Point a, Point b, const Paint& p);
    void draw_path(const Path& path, const Paint& p);

    enum class ShadowMode {
        Normal,   // complete shadow
        Hollow,   // skip the solid interior: use when an opaque shape is drawn over it afterwards
    };
    // Soft drop shadow of the rounded rectangle `r` (Gaussian blur; `blur_sigma` in logical px,
    // the visible falloff extends ~3 sigma beyond the shape). `spread` grows the shape first.
    // Rendered once per (size, radius, sigma, colour, scale) and then blitted from a cache.
    // `opacity` scales the whole shadow without invalidating the cache (use it to animate).
    void draw_shadow(const Rect& r, float radius, Color color, float blur_sigma,
                     Point offset = {}, float spread = 0, ShadowMode mode = ShadowMode::Normal,
                     float opacity = 1);

    // Draw `img` scaled into `dst`. With `tint`, the image's alpha is used as a mask (symbolic icons).
    void draw_image(const Image& img, const Rect& dst, std::optional<Color> tint = std::nullopt);
    void draw_text(const TextLayout& text, Point top_left);
    // Convenience: lay out and draw a single string (no wrapping).
    Size draw_text(std::string_view utf8, Point top_left, const TextStyle& style);

    // Escape hatch: the engine canvas (SkCanvas*). Requires including Skia headers yourself.
    void* native() const { return c_; }

private:
    void* c_;
};

} // namespace uitk
