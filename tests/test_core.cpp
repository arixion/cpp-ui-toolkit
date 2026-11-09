// Core unit tests (no display needed).
#include <cstdio>
#include <thread>
#include <vector>

#include "uitk/core/application.h"
#include "uitk/core/color.h"
#include "uitk/core/geometry.h"

using namespace uitk;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static void test_geometry() {
    Rect r{10, 20, 100, 50};
    CHECK(r.right() == 110 && r.bottom() == 70);
    CHECK(r.contains({10, 20}));
    CHECK(!r.contains({110, 20}));            // right edge exclusive
    CHECK(r.center() == (Point{60, 45}));
    CHECK(r.inset(10, 5) == (Rect{20, 25, 80, 40}));
    CHECK(r.inset(100, 0).width == 0);        // never negative
    CHECK(r.intersect({50, 0, 100, 30}) == (Rect{50, 20, 60, 10}));
    CHECK(r.intersect({500, 500, 1, 1}).empty());
    CHECK(Rect::from_ltrb(1, 2, 4, 6) == (Rect{1, 2, 3, 4}));
}

static void test_color() {
    CHECK(Color::hex(0x3584e4) == Color::rgb(0x35, 0x84, 0xe4));
    CHECK(Color::rgba(1, 2, 3, 4).argb() == 0x04010203u);
    CHECK(Color::rgb(9, 9, 9).with_alpha(7).a == 7);
}

static void test_loop() {
    Application app;
    std::vector<int> order;

    app.post([&] { order.push_back(1); });
    int repeats = 0;
    TimerId rep = app.add_timer(std::chrono::milliseconds(5), [&] { ++repeats; }, true);
    TimerId cancelled = app.add_timer(std::chrono::milliseconds(1), [&] { order.push_back(99); });
    app.cancel_timer(cancelled);
    app.add_timer(std::chrono::milliseconds(30), [&] {
        order.push_back(2);
        app.cancel_timer(rep);
        // Cross-thread post must wake the loop.
        std::thread([&] { app.post([&] { order.push_back(3); app.quit(7); }); }).detach();
    });

    int code = app.run();
    CHECK(code == 7);
    CHECK((order == std::vector<int>{1, 2, 3}));
    CHECK(repeats >= 3);
    CHECK(!app.running());
}

int main() {
    test_geometry();
    test_color();
    test_loop();
    if (failures) { fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    printf("test_core: all checks passed\n");
    return 0;
}
