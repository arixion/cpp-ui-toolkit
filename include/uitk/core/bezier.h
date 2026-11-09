// Cubic Bezier curves: one implementation for every user.
//
//  - CubicBezier : general 2D curve with arbitrary control points. Evaluation, derivative,
//                  splitting (de Casteljau), tight bounds and arc length. Used for paths/shapes.
//  - UnitBezier  : the "timing function" form with P0=(0,0) and P3=(1,1) (CSS cubic-bezier),
//                  with a robust solver for y(x). Used by Easing; usable for any 1D curve
//                  (e.g. tone curves, custom interpolation profiles).
#pragma once
#include <utility>

#include "uitk/core/geometry.h"

namespace uitk {

struct CubicBezier {
    Point p0, p1, p2, p3;

    Point at(float t) const;            // position at parameter t in [0,1]
    Point derivative(float t) const;    // tangent (not normalized)
    // Split at t into two curves that together trace exactly this one.
    std::pair<CubicBezier, CubicBezier> split(float t) const;
    // Tight axis-aligned bounds (control points excluded unless on the curve).
    Rect bounds() const;
    // Arc length by adaptive Gauss-Legendre quadrature.
    float length() const;
    // Parameter t at which the curve has travelled `distance` along its length (for even spacing).
    float t_at_length(float distance) const;
};

class UnitBezier {
public:
    // Control points (x1, y1) and (x2, y2); x values are clamped to [0, 1] so x(t) is monotonic.
    UnitBezier(float x1, float y1, float x2, float y2);

    float x(float t) const { return ((ax_ * t + bx_) * t + cx_) * t; }
    float y(float t) const { return ((ay_ * t + by_) * t + cy_) * t; }
    float dx(float t) const { return (3 * ax_ * t + 2 * bx_) * t + cx_; }

    // Parameter t with x(t) == x (Newton-Raphson with bisection fallback, as browsers do).
    float solve_t(float x, float epsilon = 1e-6f) const;
    // The curve as a function y(x): this is what timing functions evaluate.
    float operator()(float x) const { return y(solve_t(x)); }

private:
    // Polynomial coefficients (Horner form) for both components.
    float ax_, bx_, cx_, ay_, by_, cy_;
};

} // namespace uitk
