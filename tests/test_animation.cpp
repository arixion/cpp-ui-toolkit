// Animation core tests (no display needed).
#include <cmath>
#include <cstdio>

#include "uitk/core/animation.h"

using namespace uitk;
using namespace std::chrono_literals;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

static void test_easing() {
    for (Easing e : {Easing::linear(), Easing::ease(), Easing::ease_in(), Easing::ease_out(), Easing::ease_in_out(),
                     Easing::ease_out_quad(), Easing::ease_out_cubic(), Easing::ease_in_out_cubic()}) {
        CHECK_NEAR(e(0.f), 0.f, 1e-4f);
        CHECK_NEAR(e(1.f), 1.f, 1e-4f);
        float prev = -1;
        for (int i = 0; i <= 100; ++i) {          // all presets are monotonic
            float v = e(i / 100.f);
            CHECK(v >= prev - 1e-4f);
            prev = v;
        }
    }
    // Reference values of CSS cubic-bezier curves (as computed by browsers).
    CHECK_NEAR(Easing::ease()(0.5f), 0.8024f, 2e-3f);
    CHECK_NEAR(Easing::ease_in_out()(0.5f), 0.5f, 1e-3f);
    CHECK_NEAR(Easing::ease_out_quad()(0.5f), 0.75f, 1e-6f);
    // Overshooting bezier (y > 1 allowed), e.g. a "back" curve.
    float peak = 0;
    Easing back = Easing::cubic_bezier(0.34f, 1.56f, 0.64f, 1.f);
    for (int i = 0; i <= 100; ++i) peak = std::max(peak, back(i / 100.f));
    CHECK(peak > 1.05f);
}

static void test_animated() {
    FrameClock clock;
    Animated<float> a{0.f};
    TimePoint t0 = AnimClock::now();
    a.set(100.f, {Millis(200), Easing::linear()});

    clock.begin_frame(t0 + 100ms);
    float mid = a.value(clock);
    CHECK(mid > 40 && mid < 60);                  // ~50 at half time (linear)
    CHECK(clock.wants_frame());                   // running -> asks for the next frame

    clock.begin_frame(t0 + 300ms);
    CHECK_NEAR(a.value(clock), 100.f, 1e-4f);
    CHECK(!clock.wants_frame());                  // settled -> no more frames

    // Retargeting mid-flight starts from the current value: no jump.
    Animated<float> b{0.f};
    b.set(100.f, {Millis(1000), Easing::linear()});
    TimePoint mid_time = AnimClock::now();
    clock.begin_frame(mid_time);
    float before = b.value(clock);
    b.set(0.f, {Millis(1000), Easing::linear()});
    clock.begin_frame(AnimClock::now());
    float after = b.value(clock);
    CHECK(std::fabs(after - before) < 5.f);

    // Setting the same target again does not restart the transition.
    Animated<float> c{0.f};
    c.set(1.f, {Millis(100)});
    c.set(1.f, {Millis(5000)});
    clock.begin_frame(AnimClock::now() + 150ms);
    CHECK_NEAR(c.value(clock), 1.f, 1e-4f);

    // Delay holds the start value.
    Animated<float> d{0.f};
    d.set(1.f, {Millis(100), Easing::linear(), Millis(500)});
    clock.begin_frame(AnimClock::now() + 200ms);
    CHECK_NEAR(d.value(clock), 0.f, 1e-4f);

    // Colours interpolate premultiplied: fading to transparent keeps the hue (no darkening).
    Color half = lerp(Color::rgb(255, 0, 0), Color::transparent(), 0.5f);
    CHECK(half.r == 255 && half.g == 0 && half.b == 0);
    CHECK(half.a == 128);
    CHECK(lerp(Color::rgb(0, 0, 0), Color::rgb(255, 255, 255), 0.5f) == Color::rgb(128, 128, 128));
}

static void test_reduced_motion() {
    set_animations_enabled(false);
    FrameClock clock;
    clock.begin_frame(AnimClock::now());
    Animated<float> a{0.f};
    a.set(1.f, {Millis(500)});
    CHECK_NEAR(a.value(clock), 1.f, 1e-6f);       // instant
    CHECK(!clock.wants_frame());
    Spring s{0.f};
    s.set_target(10.f);
    CHECK_NEAR(s.value(clock), 10.f, 1e-6f);
    set_animations_enabled(true);
}

static void test_spring() {
    // Critically damped: converges without overshoot.
    FrameClock clock;
    Spring s{0.f, SpringParams::critical(400)};
    s.set_target(1.f);
    TimePoint t = AnimClock::now();
    float max_v = 0;
    for (int i = 1; i <= 120; ++i) {              // 2 s at 60 Hz
        clock.begin_frame(t + std::chrono::microseconds(16667 * i));
        max_v = std::max(max_v, s.value(clock));
    }
    CHECK(max_v <= 1.0005f);
    CHECK(s.resting());
    CHECK_NEAR(s.value(clock), 1.f, 1e-6f);

    // Under-damped: overshoots, then still settles.
    Spring b{0.f, SpringParams::bouncy()};
    b.set_target(1.f);
    max_v = 0;
    for (int i = 1; i <= 240; ++i) {
        clock.begin_frame(t + std::chrono::microseconds(16667 * i));
        max_v = std::max(max_v, b.value(clock));
    }
    CHECK(max_v > 1.05f);
    CHECK(b.resting());

    // Velocity carries over when retargeted mid-motion (no instantaneous stop).
    Spring r{0.f};
    r.set_target(1.f);
    for (int i = 1; i <= 6; ++i) { clock.begin_frame(t + std::chrono::microseconds(16667 * i)); r.value(clock); }
    float v_before = r.velocity();
    r.set_target(-1.f);
    CHECK(v_before > 0 && r.velocity() == v_before);
}

static void test_looping() {
    FrameClock clock;
    Looping loop(Millis(1000));
    clock.begin_frame(AnimClock::now());
    CHECK(loop.phase(clock) == 0.f && !clock.wants_frame());   // stopped
    loop.start();
    clock.begin_frame(AnimClock::now() + 1250ms);
    float ph = loop.phase(clock);
    CHECK(ph > 0.2f && ph < 0.3f);
    CHECK(clock.wants_frame());
    loop.stop();
    clock.begin_frame(AnimClock::now());
    CHECK(loop.phase(clock) == 0.f && !clock.wants_frame());
}

int main() {
    test_easing();
    test_animated();
    test_reduced_motion();
    test_spring();
    test_looping();
    if (failures) { fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    printf("test_animation: all checks passed\n");
    return 0;
}
