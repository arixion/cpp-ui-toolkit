// Widget layer tests: layout, event routing, button behaviour, focus. Runs offscreen.
#include <cmath>
#include <cstdio>

#include "uitk/core/bezier.h"
#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/button.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/list.h"
#include "uitk/widgets/notification.h"
#include "uitk/widgets/split_button.h"
#include "uitk/widgets/status_bar.h"
#include "uitk/widgets/window_shell.h"

using namespace uitk;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

static PointerEvent pointer(PointerEvent::Type t, Point p, PointerButton b = PointerButton::Left) {
    PointerEvent e; e.type = t; e.pos = p; e.button = b; e.click_count = 1;
    return e;
}
static KeyEvent keyev(uint32_t sym, bool pressed, uint32_t mods = 0) {
    KeyEvent k; k.keysym = sym; k.pressed = pressed; k.mods = mods;
    return k;
}
static Point center_of(Widget& w) { return w.to_window(w.local_rect().center()); }

static void test_bezier_and_signal() {
    CubicBezier line{{0, 0}, {1, 1}, {2, 2}, {3, 3}};
    CHECK_NEAR(line.length(), std::sqrt(18.f), 1e-3f);
    CHECK_NEAR(line.at(0.5f).x, 1.5f, 1e-5f);
    auto [a, b] = CubicBezier{{0, 0}, {0, 10}, {10, 10}, {10, 0}}.split(0.5f);
    CHECK_NEAR(a.p3.x, b.p0.x, 1e-6f);
    CHECK_NEAR(a.p3.y, 7.5f, 1e-5f);                        // apex of the symmetric arch
    Rect bb = CubicBezier{{0, 0}, {0, 10}, {10, 10}, {10, 0}}.bounds();
    CHECK_NEAR(bb.height, 7.5f, 1e-4f);                    // tight: excludes control points
    CHECK_NEAR(UnitBezier(0.25f, 0.1f, 0.25f, 1.f)(0.5f), 0.8024f, 2e-3f);

    Signal<int> s;
    int sum = 0;
    ConnectionId id = s.connect([&](int v) { sum += v; });
    s.connect([&](int v) { sum += 10 * v; });
    s.emit(1);
    CHECK(sum == 11);
    s.disconnect(id);
    s(2);
    CHECK(sum == 31);
}

static void test_layout() {
    WidgetHost host;
    Box& col = host.emplace_root<Box>(Orientation::Vertical, 10.f);
    col.set_padding(Insets{5, 5, 5, 5});
    Widget& a = col.add<Widget>(); a.set_min_size({100, 20});
    Widget& b = col.add<Widget>(); b.set_min_size({50, 30}); b.set_expand(false, true);
    Widget& c = col.add<Widget>(); c.set_min_size({60, 10}); c.set_align(Align::End, Align::Fill);
    host.layout({200, 200});
    CHECK(a.bounds() == (Rect{5, 5, 190, 20}));            // Fill: full width
    // b expands: 190 inner height - 20 - 10 - 2*10 spacing = 140
    CHECK(b.bounds() == (Rect{5, 35, 190, 140}));
    CHECK(c.bounds() == (Rect{135, 185, 60, 10}));         // End-aligned at natural width
    Size m = col.measure(200, host.theme());
    CHECK(m.width == 110 && m.height == 5 + 20 + 10 + 30 + 10 + 10 + 5);

    Widget& f = col.add<Widget>();
    f.set_fixed_size({22, 22});
    CHECK(f.measure_clamped(500, host.theme()) == (Size{22, 22}));

    // Wrapping text gets taller when narrower.
    Label& wrap = col.add<Label>("one two three four five six seven eight nine ten eleven twelve");
    wrap.set_wrap(true);
    CHECK(wrap.measure(80, host.theme()).height > wrap.measure(2000, host.theme()).height);
}

static void test_button_clicks() {
    WindowShell shell(nullptr, "test");
    Button& btn = shell.content().add<Button>("Press me");
    Button& off = shell.content().add<Button>("Disabled");
    off.set_enabled(false);
    int clicks = 0, off_clicks = 0;
    btn.clicked.connect([&] { ++clicks; });
    off.clicked.connect([&] { ++off_clicks; });
    shell.host().layout({400, 300});
    Point p = center_of(btn);

    shell.handle_event(pointer(PointerEvent::Type::Move, p));
    CHECK(btn.state() & STATE_HOVERED);
    shell.handle_event(pointer(PointerEvent::Type::Down, p));
    CHECK(btn.state() & STATE_PRESSED);
    CHECK(btn.has_focus() && !(btn.state() & STATE_FOCUS_VISIBLE));   // click focus: no ring
    shell.handle_event(pointer(PointerEvent::Type::Up, p));
    CHECK(clicks == 1);
    CHECK(!(btn.state() & STATE_PRESSED));

    // Press, drag off (un-presses), release outside: no click.
    shell.handle_event(pointer(PointerEvent::Type::Down, p));
    shell.handle_event(pointer(PointerEvent::Type::Move, {390, 290}));
    CHECK(!(btn.state() & STATE_PRESSED) && !(btn.state() & STATE_HOVERED));
    shell.handle_event(pointer(PointerEvent::Type::Move, p));   // back on: pressed again
    CHECK(btn.state() & STATE_PRESSED);
    shell.handle_event(pointer(PointerEvent::Type::Move, {390, 290}));
    shell.handle_event(pointer(PointerEvent::Type::Up, {390, 290}));
    CHECK(clicks == 1);

    // Disabled buttons ignore the pointer.
    Point q = center_of(off);
    shell.handle_event(pointer(PointerEvent::Type::Down, q));
    shell.handle_event(pointer(PointerEvent::Type::Up, q));
    CHECK(off_clicks == 0);
    CHECK(off.state() & STATE_DISABLED);

    // Keyboard: Space clicks on release, Enter on press.
    btn.grab_focus();
    shell.handle_event(keyev(0x20, true));
    CHECK(clicks == 1);
    shell.handle_event(keyev(0x20, false));
    CHECK(clicks == 2);
    shell.handle_event(keyev(key::Return, true));
    CHECK(clicks == 3);

    // Right-click does not click.
    shell.handle_event(pointer(PointerEvent::Type::Down, p, PointerButton::Right));
    shell.handle_event(pointer(PointerEvent::Type::Up, p, PointerButton::Right));
    CHECK(clicks == 3);
}

static void test_focus_chain() {
    WindowShell shell(nullptr, "test");
    Button& a = shell.content().add<Button>("A");
    Button& b = shell.content().add<Button>("B");
    Button& c = shell.content().add<Button>("C");
    b.set_enabled(false);
    shell.host().layout({400, 300});
    // Window controls are not focusable; disabled widgets are skipped.
    shell.handle_event(keyev(key::Tab, true));
    CHECK(a.has_focus() && (a.state() & STATE_FOCUS_VISIBLE));
    shell.handle_event(keyev(key::Tab, true));
    CHECK(c.has_focus());
    shell.handle_event(keyev(key::Tab, true));
    CHECK(a.has_focus());                                   // wraps around
    shell.handle_event(keyev(key::Tab, true, mod::Shift));
    CHECK(c.has_focus());
    CHECK(!shell.header_bar().close_button().has_focus());

    // Removing the focused widget clears focus safely.
    shell.content().remove_child(&c);
    CHECK(shell.host().focus() == nullptr);
}

static void test_header_close() {
    WindowShell shell(nullptr, "test");
    int closes = 0;
    shell.close_requested.connect([&] { ++closes; });
    shell.host().layout({1000, 300});
    Point p = center_of(shell.header_bar().close_button());
    shell.handle_event(pointer(PointerEvent::Type::Down, p));
    shell.handle_event(pointer(PointerEvent::Type::Up, p));
    CHECK(closes == 1);
    shell.handle_event(CloseEvent{});
    CHECK(closes == 2);
    // The title (command center) stays centred on the window when there is room.
    Label& t = shell.header_bar().title_label();
    CHECK(std::fabs(t.to_window(t.local_rect().center()).x - 500) <= 1);
}

static void test_vscode_controls() {
    WindowShell shell(nullptr, "test");
    Box& col = shell.content();
    SplitButton& split = col.add<SplitButton>("New Profile");
    ListBox& list = col.add<ListBox>();
    ListRow& r1 = list.add<ListRow>("One");
    ListRow& r2 = list.add<ListRow>("Two");
    ListRow& r3 = list.add<ListRow>("Three");
    Button& action = r2.actions().add<Button>("act", ButtonStyle::Flat);
    Notification& note = col.add<Notification>("Hello", "test");
    StatusBar& bar = col.add<StatusBar>();
    Button& item = bar.add_end("UTF-8");
    shell.host().layout({600, 500});

    int main_clicks = 0, menu_clicks = 0, closes = 0, item_clicks = 0, selections = 0;
    split.clicked.connect([&] { ++main_clicks; });
    split.menu_requested.connect([&] { ++menu_clicks; });
    note.closed.connect([&] { ++closes; });
    item.clicked.connect([&] { ++item_clicks; });
    list.selection_changed.connect([&](ListRow&) { ++selections; });

    auto click = [&](Widget& w) {
        Point p = center_of(w);
        shell.handle_event(pointer(PointerEvent::Type::Move, p));
        shell.handle_event(pointer(PointerEvent::Type::Down, p));
        shell.handle_event(pointer(PointerEvent::Type::Up, p));
    };
    click(split.main_button());
    click(split.menu_button());
    CHECK(main_clicks == 1 && menu_clicks == 1);
    // The two halves share one outline: only outer corners are rounded, chevron on the right.
    CHECK(split.menu_button().bounds().x > split.main_button().bounds().x);

    // Selection by pointer; actions show while the row is hovered or selected.
    CHECK(!r2.actions().visible());
    click(r2);
    CHECK(r2.selected() && list.selected() == &r2 && selections == 1);
    CHECK(r2.actions().visible());
    shell.host().layout({600, 500});   // the newly shown actions get laid out (as before a frame)
    // Hovering the action keeps the row "hovered within" (actions stay visible).
    shell.handle_event(pointer(PointerEvent::Type::Move, center_of(action)));
    CHECK(r2.state() & STATE_HOVER_WITHIN);
    CHECK(action.state() & STATE_HOVERED);
    CHECK(!(r2.state() & STATE_HOVERED));

    // Arrow keys move the selection and the focus.
    shell.handle_event(keyev(key::Down, true));
    CHECK(r3.selected() && r3.has_focus() && !r2.selected());
    shell.handle_event(keyev(key::Home, true));
    CHECK(r1.selected());
    CHECK(selections == 3);

    click(item);
    CHECK(item_clicks == 1);

    // Notification close button is the last button in its first row.
    Button* close = nullptr;
    for (auto& c : note.children().front()->children()) if (auto* b = dynamic_cast<Button*>(c.get())) close = b;
    CHECK(close != nullptr);
    if (close) click(*close);
    CHECK(closes == 1);

    // Window controls follow the theme's chrome style.
    shell.set_theme(Theme::vscode_dark(), false);
    shell.host().layout({601, 500});
    Button& cb = shell.header_bar().close_button();
    CHECK(cb.size().width == Theme::vscode_dark().window_button_width);
    shell.set_theme(Theme::adwaita_dark(), false);
    shell.host().layout({602, 500});
    CHECK(cb.size().width == Theme::adwaita_dark().window_button_height);   // round, square slot
}

int main() {
    test_vscode_controls();
    test_bezier_and_signal();
    test_layout();
    test_button_clicks();
    test_focus_chain();
    test_header_close();
    if (failures) { fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    printf("test_widgets: all checks passed\n");
    return 0;
}
