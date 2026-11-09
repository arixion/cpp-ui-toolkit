// Animation primitives.
//
//  - Easing          timing curves (CSS-compatible cubic-bezier + common presets)
//  - FrameClock      per-window frame time; animations ask it for another frame while running
//  - Transition      duration + easing (+ delay) of a change
//  - Animated<T>     a value that transitions to new targets; retargeting mid-flight is seamless
//  - Spring          physically based motion (velocity carries over when the target changes)
//  - Looping         endless phase 0..1 for spinners / indeterminate progress
//
// Animations are pull-based: read value(clock) while painting. Any running animation calls
// clock.request_frame(), and the window schedules the next frame (vsync-paced) only while that
// happens, so an idle UI renders nothing and uses no CPU.
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

#include "uitk/core/bezier.h"
#include "uitk/core/color.h"
#include "uitk/core/geometry.h"

namespace uitk {

using AnimClock = std::chrono::steady_clock;
using TimePoint = AnimClock::time_point;
using Millis    = std::chrono::milliseconds;

// ---------------------------------------------------------------------------- reduced motion

// Global switch; when off, transitions complete instantly (accessibility: "reduce motion").
// Defaults to on unless the environment sets UITK_REDUCED_MOTION=1.
bool animations_enabled();
void set_animations_enabled(bool enabled);

// ---------------------------------------------------------------------------- easing

class Easing {
public:
    static Easing linear()            { return Easing(Kind::Linear); }
    // CSS named curves.
    static Easing ease()              { return cubic_bezier(0.25f, 0.1f, 0.25f, 1.f); }
    static Easing ease_in()           { return cubic_bezier(0.42f, 0.f, 1.f, 1.f); }
    static Easing ease_out()          { return cubic_bezier(0.f, 0.f, 0.58f, 1.f); }
    static Easing ease_in_out()       { return cubic_bezier(0.42f, 0.f, 0.58f, 1.f); }
    // Polynomial curves (cheap, exact).
    static Easing ease_out_quad()     { return Easing(Kind::OutQuad); }    // GNOME/libadwaita default
    static Easing ease_out_cubic()    { return Easing(Kind::OutCubic); }
    static Easing ease_in_out_cubic() { return Easing(Kind::InOutCubic); }
    // Arbitrary CSS cubic-bezier(x1, y1, x2, y2); x1, x2 are clamped to [0, 1].
    static Easing cubic_bezier(float x1, float y1, float x2, float y2);

    float operator()(float t) const;   // t in [0,1] -> eased progress (may overshoot for beziers)

private:
    enum class Kind { Linear, OutQuad, OutCubic, InOutCubic, Bezier };
    explicit Easing(Kind k) : kind_(k) {}
    Kind       kind_;
    UnitBezier bezier_{0, 0, 1, 1};   // shared curve solver (uitk/core/bezier.h)
};

struct Transition {
    Millis duration{200};
    Easing easing = Easing::ease_out_quad();
    Millis delay{0};

    static Transition instant() { return {Millis(0)}; }
};

// ---------------------------------------------------------------------------- frame clock

class FrameClock {
public:
    // Time of the frame being painted (all animations in a frame agree on it). Outside a frame
    // this is the time of the last frame; offscreen clocks may be frozen at any time.
    TimePoint now() const { return now_; }
    void begin_frame(TimePoint t) { now_ = t; wants_frame_ = false; }
    void request_frame() { wants_frame_ = true; }      // called by running animations
    bool wants_frame() const { return wants_frame_; }

    // A clock that reports "far in the future": every animation reads its final value.
    // Used for offscreen rendering / snapshots.
    static FrameClock settled() { FrameClock c; c.now_ = TimePoint(TimePoint::duration::max() / 2); return c; }

private:
    TimePoint now_ = AnimClock::now();
    bool wants_frame_ = false;
};

// ---------------------------------------------------------------------------- interpolation

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Point lerp(Point a, Point b, float t) { return {lerp(a.x, b.x, t), lerp(a.y, b.y, t)}; }
inline Size  lerp(Size a, Size b, float t)   { return {lerp(a.width, b.width, t), lerp(a.height, b.height, t)}; }
inline Rect  lerp(const Rect& a, const Rect& b, float t) {
    return {lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.width, b.width, t), lerp(a.height, b.height, t)};
}
// Colours interpolate premultiplied, so fading to/from transparent does not darken midway.
Color lerp(Color a, Color b, float t);

// ---------------------------------------------------------------------------- Animated<T>

template <class T>
class Animated {
public:
    Animated() = default;
    explicit Animated(T value) : from_(value), to_(value) {}

    // Animate from the current (possibly mid-flight) value to `target`.
    void set(T target, const Transition& tr = {}) {
        if (target == to_) return;   // already there or already heading there
        TimePoint now = AnimClock::now();
        from_ = sample(now);
        to_ = target;
        tr_ = animations_enabled() ? tr : Transition::instant();
        start_ = now + tr_.delay;
    }
    void jump(T value) { from_ = to_ = value; tr_ = Transition::instant(); }

    const T& target() const { return to_; }

    // Value for the frame being painted; keeps frames coming while the transition runs.
    T value(FrameClock& clock) const {
        if (running_at(clock.now())) clock.request_frame();
        return sample(clock.now());
    }
    bool running(const FrameClock& clock) const { return running_at(clock.now()); }

private:
    bool running_at(TimePoint t) const {
        return tr_.duration.count() > 0 && t < start_ + tr_.duration && !(from_ == to_);
    }
    T sample(TimePoint t) const {
        if (tr_.duration.count() <= 0 || t >= start_ + tr_.duration) return to_;
        if (t <= start_) return from_;
        float p = std::chrono::duration<float>(t - start_).count() /
                  std::chrono::duration<float>(tr_.duration).count();
        using uitk::lerp;
        return lerp(from_, to_, tr_.easing(std::clamp(p, 0.f, 1.f)));
    }

    T from_{}, to_{};
    TimePoint start_{};
    Transition tr_ = Transition::instant();
};

// ---------------------------------------------------------------------------- Spring

struct SpringParams {
    float stiffness = 380;   // k
    float damping   = 34;    // c
    float mass      = 1;     // m
    // Critically damped (fastest settle without overshoot) for a given stiffness.
    static SpringParams critical(float stiffness = 380, float mass = 1) {
        return {stiffness, 2.f * std::sqrt(stiffness * mass), mass};
    }
    static SpringParams bouncy() { return {320, 14, 1}; }
};

// Damped harmonic oscillator. Unlike timed transitions, a spring keeps its velocity when the
// target changes mid-motion (natural for drags, toggles, scrolling, interruptible motion).
class Spring {
public:
    using Params = SpringParams;

    Spring() = default;
    explicit Spring(float value, Params p = Params{}) : x_(value), target_(value), p_(p) {}

    void set_target(float target);
    void jump(float value) { x_ = target_ = value; v_ = 0; resting_ = true; }
    void set_velocity(float v) { v_ = v; resting_ = false; }   // e.g. fling at drag release
    float target() const { return target_; }

    float value(FrameClock& clock);   // advances the simulation to clock.now()
    float velocity() const { return v_; }
    bool  resting() const { return resting_; }

private:
    float x_ = 0, v_ = 0, target_ = 0;
    Params p_;
    TimePoint last_{};
    bool resting_ = true;
};

// ---------------------------------------------------------------------------- Looping

// Endless animation (spinner rotation, indeterminate progress). While running it requests a
// frame every vsync, so stop it as soon as the indicator is hidden.
class Looping {
public:
    explicit Looping(Millis period = Millis(1000)) : period_(period) {}
    void start() { if (!running_) { running_ = true; start_ = AnimClock::now(); } }
    void stop() { running_ = false; }
    bool running() const { return running_; }

    // Phase in [0,1); 0 when stopped (or when animations are disabled).
    float phase(FrameClock& clock) const;

private:
    Millis    period_;
    TimePoint start_{};
    bool      running_ = false;
};

} // namespace uitk
