#include "uitk/core/bezier.h"

#include <algorithm>
#include <cmath>

namespace uitk {

// ---------------------------------------------------------------------------- CubicBezier

Point CubicBezier::at(float t) const {
    float u = 1 - t;
    float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
    return {a * p0.x + b * p1.x + c * p2.x + d * p3.x, a * p0.y + b * p1.y + c * p2.y + d * p3.y};
}

Point CubicBezier::derivative(float t) const {
    float u = 1 - t;
    float a = 3 * u * u, b = 6 * u * t, c = 3 * t * t;
    return {a * (p1.x - p0.x) + b * (p2.x - p1.x) + c * (p3.x - p2.x),
            a * (p1.y - p0.y) + b * (p2.y - p1.y) + c * (p3.y - p2.y)};
}

std::pair<CubicBezier, CubicBezier> CubicBezier::split(float t) const {
    auto mix = [t](Point a, Point b) { return Point{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; };
    Point p01 = mix(p0, p1), p12 = mix(p1, p2), p23 = mix(p2, p3);
    Point p012 = mix(p01, p12), p123 = mix(p12, p23);
    Point p = mix(p012, p123);
    return {{p0, p01, p012, p}, {p, p123, p23, p3}};
}

Rect CubicBezier::bounds() const {
    float minx = std::min(p0.x, p3.x), maxx = std::max(p0.x, p3.x);
    float miny = std::min(p0.y, p3.y), maxy = std::max(p0.y, p3.y);
    // Extrema where a derivative component is zero: solve the quadratic per axis.
    auto extrema = [&](float a0, float a1, float a2, float a3, bool is_x) {
        float a = -a0 + 3 * a1 - 3 * a2 + a3, b = 2 * (a0 - 2 * a1 + a2), c = a1 - a0;
        float roots[2]; int n = 0;
        if (std::fabs(a) < 1e-9f) {
            if (std::fabs(b) > 1e-9f) roots[n++] = -c / b;
        } else {
            float disc = b * b - 4 * a * c;
            if (disc >= 0) {
                float sq = std::sqrt(disc);
                roots[n++] = (-b + sq) / (2 * a);
                roots[n++] = (-b - sq) / (2 * a);
            }
        }
        for (int i = 0; i < n; ++i) {
            if (roots[i] <= 0 || roots[i] >= 1) continue;
            Point p = at(roots[i]);
            if (is_x) { minx = std::min(minx, p.x); maxx = std::max(maxx, p.x); }
            else      { miny = std::min(miny, p.y); maxy = std::max(maxy, p.y); }
        }
    };
    extrema(p0.x, p1.x, p2.x, p3.x, true);
    extrema(p0.y, p1.y, p2.y, p3.y, false);
    return Rect::from_ltrb(minx, miny, maxx, maxy);
}

namespace {
// 5-point Gauss-Legendre on [a,b] for |B'(t)|.
float gauss_length(const CubicBezier& c, float a, float b) {
    static const float x[5] = {0.f, -0.5384693101f, 0.5384693101f, -0.9061798459f, 0.9061798459f};
    static const float w[5] = {0.5688888889f, 0.4786286705f, 0.4786286705f, 0.2369268851f, 0.2369268851f};
    float half = (b - a) / 2, mid = (a + b) / 2, sum = 0;
    for (int i = 0; i < 5; ++i) {
        Point d = c.derivative(mid + half * x[i]);
        sum += w[i] * std::sqrt(d.x * d.x + d.y * d.y);
    }
    return sum * half;
}
float adaptive_length(const CubicBezier& c, float a, float b, float whole, int depth) {
    float m = (a + b) / 2;
    float left = gauss_length(c, a, m), right = gauss_length(c, m, b);
    if (depth >= 8 || std::fabs(left + right - whole) < 1e-4f * std::max(1.f, whole)) return left + right;
    return adaptive_length(c, a, m, left, depth + 1) + adaptive_length(c, m, b, right, depth + 1);
}
} // namespace

float CubicBezier::length() const { return adaptive_length(*this, 0, 1, gauss_length(*this, 0, 1), 0); }

float CubicBezier::t_at_length(float distance) const {
    float total = length();
    if (distance <= 0) return 0;
    if (distance >= total) return 1;
    // Bisection on the monotonic arc-length function.
    float lo = 0, hi = 1;
    for (int i = 0; i < 30; ++i) {
        float mid = (lo + hi) / 2;
        float len = adaptive_length(*this, 0, mid, gauss_length(*this, 0, mid), 0);
        (len < distance ? lo : hi) = mid;
    }
    return (lo + hi) / 2;
}

// ---------------------------------------------------------------------------- UnitBezier

UnitBezier::UnitBezier(float x1, float y1, float x2, float y2) {
    x1 = std::clamp(x1, 0.f, 1.f);
    x2 = std::clamp(x2, 0.f, 1.f);
    // B(t) = 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3, expanded to a t^3 + b t^2 + c t.
    cx_ = 3 * x1; bx_ = 3 * (x2 - x1) - cx_; ax_ = 1 - cx_ - bx_;
    cy_ = 3 * y1; by_ = 3 * (y2 - y1) - cy_; ay_ = 1 - cy_ - by_;
}

float UnitBezier::solve_t(float xv, float eps) const {
    if (xv <= 0) return 0;
    if (xv >= 1) return 1;
    float t = xv;
    for (int i = 0; i < 8; ++i) {          // Newton-Raphson: fast when the slope is healthy
        float err = x(t) - xv;
        if (std::fabs(err) < eps) return t;
        float d = dx(t);
        if (std::fabs(d) < 1e-6f) break;
        t -= err / d;
    }
    float lo = 0, hi = 1;                   // bisection: always converges (x is monotonic)
    t = xv;
    for (int i = 0; i < 40; ++i) {
        float v = x(t);
        if (std::fabs(v - xv) < eps) break;
        (v < xv ? lo : hi) = t;
        t = (lo + hi) / 2;
    }
    return t;
}

} // namespace uitk
