// Basic geometry types. All public coordinates are logical pixels (scale-independent).
#pragma once
#include <algorithm>

namespace uitk {

struct Point {
    float x = 0, y = 0;
    friend constexpr Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
    friend constexpr Point operator-(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
    friend constexpr bool operator==(Point, Point) = default;
};

struct Size {
    float width = 0, height = 0;
    constexpr bool empty() const { return width <= 0 || height <= 0; }
    friend constexpr bool operator==(Size, Size) = default;
};

struct Insets {
    float left = 0, top = 0, right = 0, bottom = 0;
    constexpr bool empty() const { return left == 0 && top == 0 && right == 0 && bottom == 0; }
    friend constexpr bool operator==(Insets, Insets) = default;
};

// Per-corner radii (top-left, top-right, bottom-right, bottom-left).
struct CornerRadii {
    float tl = 0, tr = 0, br = 0, bl = 0;
    static constexpr CornerRadii all(float r) { return {r, r, r, r}; }
    static constexpr CornerRadii left(float r) { return {r, 0, 0, r}; }    // left end of a group
    static constexpr CornerRadii right(float r) { return {0, r, r, 0}; }   // right end of a group
    friend constexpr bool operator==(CornerRadii, CornerRadii) = default;
};

struct Rect {
    float x = 0, y = 0, width = 0, height = 0;

    static constexpr Rect from_ltrb(float l, float t, float r, float b) { return {l, t, r - l, b - t}; }
    static constexpr Rect from_size(Size s) { return {0, 0, s.width, s.height}; }

    constexpr float left() const   { return x; }
    constexpr float top() const    { return y; }
    constexpr float right() const  { return x + width; }
    constexpr float bottom() const { return y + height; }
    constexpr Point origin() const { return {x, y}; }
    constexpr Size  size() const   { return {width, height}; }
    constexpr Point center() const { return {x + width / 2, y + height / 2}; }
    constexpr bool  empty() const  { return width <= 0 || height <= 0; }

    constexpr bool contains(Point p) const {
        return p.x >= x && p.y >= y && p.x < x + width && p.y < y + height;
    }
    constexpr Rect inset(float dx, float dy) const {
        return {x + dx, y + dy, std::max(0.f, width - 2 * dx), std::max(0.f, height - 2 * dy)};
    }
    constexpr Rect intersect(const Rect& o) const {
        float l = std::max(left(), o.left()), t = std::max(top(), o.top());
        float r = std::min(right(), o.right()), b = std::min(bottom(), o.bottom());
        return (r > l && b > t) ? from_ltrb(l, t, r, b) : Rect{};
    }
    friend constexpr bool operator==(const Rect&, const Rect&) = default;
};

} // namespace uitk
