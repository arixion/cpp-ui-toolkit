#include "uitk/core/animation.h"

#include <cstdlib>
#include <cstring>

namespace uitk {

// ---------------------------------------------------------------------------- reduced motion

namespace {
bool initial_animations_enabled() {
    const char* v = getenv("UITK_REDUCED_MOTION");
    return !(v && *v && strcmp(v, "0") != 0);
}
bool g_animations_enabled = initial_animations_enabled();
} // namespace

bool animations_enabled() { return g_animations_enabled; }
void set_animations_enabled(bool enabled) { g_animations_enabled = enabled; }

// ---------------------------------------------------------------------------- easing

Easing Easing::cubic_bezier(float x1, float y1, float x2, float y2) {
    Easing e(Kind::Bezier);
    e.bezier_ = UnitBezier(x1, y1, x2, y2);
    return e;
}

float Easing::operator()(float t) const {
    t = std::clamp(t, 0.f, 1.f);
    switch (kind_) {
        case Kind::Linear:     return t;
        case Kind::OutQuad:    return 1 - (1 - t) * (1 - t);
        case Kind::OutCubic:   { float u = 1 - t; return 1 - u * u * u; }
        case Kind::InOutCubic: return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3.f) / 2;
        case Kind::Bezier:     return (t <= 0 || t >= 1) ? t : bezier_(t);
    }
    return t;
}

// ---------------------------------------------------------------------------- colour lerp

Color lerp(Color a, Color b, float t) {
    // Premultiplied interpolation, then back to straight alpha.
    float aa = a.a / 255.f, ba = b.a / 255.f;
    float alpha = aa + (ba - aa) * t;
    auto ch = [&](uint8_t ca, uint8_t cb) -> uint8_t {
        if (alpha <= 0) return 0;
        float pa = ca * aa, pb = cb * ba;
        float v = (pa + (pb - pa) * t) / alpha;
        return (uint8_t)std::clamp((int)std::lround(v), 0, 255);
    };
    return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), (uint8_t)std::lround(alpha * 255)};
}

// ---------------------------------------------------------------------------- Spring

void Spring::set_target(float target) {
    if (target == target_ && resting_) return;
    target_ = target;
    if (!animations_enabled()) { jump(target); return; }
    if (resting_) last_ = AnimClock::now();
    resting_ = false;
}

float Spring::value(FrameClock& clock) {
    if (resting_) return x_;
    TimePoint now = clock.now();
    if (now <= last_) { clock.request_frame(); return x_; }
    float dt = std::chrono::duration<float>(now - last_).count();
    last_ = now;
    dt = std::min(dt, 0.1f);   // after a stall, don't jump: resume smoothly
    // Semi-implicit Euler in 1 ms substeps: stable for UI-range stiffness.
    const float h = 0.001f;
    for (float t = 0; t < dt; t += h) {
        float step = std::min(h, dt - t);
        float force = -p_.stiffness * (x_ - target_) - p_.damping * v_;
        v_ += force / p_.mass * step;
        x_ += v_ * step;
    }
    if (std::fabs(x_ - target_) < 1e-3f && std::fabs(v_) < 1e-2f) {
        x_ = target_; v_ = 0; resting_ = true;
    } else {
        clock.request_frame();
    }
    return x_;
}

// ---------------------------------------------------------------------------- Looping

float Looping::phase(FrameClock& clock) const {
    if (!running_ || !animations_enabled() || period_.count() <= 0) return 0;
    clock.request_frame();
    auto elapsed = clock.now() - start_;
    if (elapsed.count() < 0) return 0;
    double p = std::chrono::duration<double>(elapsed).count() / std::chrono::duration<double>(period_).count();
    return (float)(p - std::floor(p));
}

} // namespace uitk
