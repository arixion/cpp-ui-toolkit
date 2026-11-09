// Basic controls: behaviour tests through real events, offscreen.
#include <cmath>
#include <cstdio>
#include <string>

#include "uitk/widgets/containers.h"
#include "uitk/widgets/dropdown.h"
#include "uitk/widgets/range.h"
#include "uitk/widgets/text_field.h"
#include "uitk/widgets/toggles.h"
#include "uitk/widgets/window_shell.h"

using namespace uitk;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

namespace {
WindowShell* g_shell = nullptr;
const Size kSize{800, 700};
void relayout() { g_shell->host().layout(kSize); }
void send(const Event& e) { g_shell->handle_event(e); }
PointerEvent ptr(PointerEvent::Type t, Point p, int clicks = 1, uint32_t mods = 0) {
    PointerEvent e; e.type = t; e.pos = p; e.button = PointerButton::Left; e.click_count = clicks; e.mods = mods;
    return e;
}
Point center(Widget& w) { return w.to_window(w.local_rect().center()); }
void click(Widget& w, int clicks = 1) {
    Point p = center(w);
    send(ptr(PointerEvent::Type::Move, p));
    send(ptr(PointerEvent::Type::Down, p, clicks));
    send(ptr(PointerEvent::Type::Up, p, clicks));
}
void press(uint32_t sym, uint32_t mods = 0) {
    KeyEvent k; k.keysym = sym; k.mods = mods; k.pressed = true; send(k);
    k.pressed = false; send(k);
}
void type(const char* s) { send(TextEvent{s}); }
} // namespace

static void test_toggles() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    Box& c = shell.content();
    CheckBox& cb = c.add<CheckBox>("Check");
    RadioGroup group;
    RadioButton& r1 = c.add<RadioButton>("A", group);
    RadioButton& r2 = c.add<RadioButton>("B", group);
    Switch& sw = c.add<Switch>("Switch");
    ToggleButton& tb = c.add<ToggleButton>("Toggle");
    relayout();

    int toggles = 0; cb.toggled.connect([&](bool) { ++toggles; });
    click(cb);
    CHECK(cb.checked() && toggles == 1);
    press(0x20);   // Space toggles the focused check box (click focused it)
    CHECK(!cb.checked() && toggles == 2);
    cb.set_indeterminate(true);
    click(cb);
    CHECK(!cb.indeterminate() && cb.checked());

    int changed = -1; group.changed.connect([&](int i) { changed = i; });
    click(r2);
    CHECK(r2.checked() && !r1.checked() && changed == 1 && group.selected() == 1);
    click(r2);   // clicking the selected radio keeps it selected
    CHECK(r2.checked());
    click(r1);
    CHECK(r1.checked() && !r2.checked() && changed == 0);

    click(sw);
    CHECK(sw.checked());
    click(tb);
    CHECK(tb.checked());
    click(tb);
    CHECK(!tb.checked());
    cb.set_enabled(false);
    bool before = cb.checked();
    click(cb);
    CHECK(cb.checked() == before);
}

static void test_slider_progress() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    Slider& s = shell.content().add<Slider>(0, 100, 50, 5);
    s.set_fixed_size({200, 0});
    ProgressBar& p = shell.content().add<ProgressBar>(0.5f);
    relayout();
    Rect b{s.to_window({0, 0}).x, s.to_window({0, 0}).y, s.size().width, s.size().height};
    send(ptr(PointerEvent::Type::Move, {b.x + 2, b.center().y}));
    send(ptr(PointerEvent::Type::Down, {b.x + 2, b.center().y}));
    CHECK(s.value() == 0);
    send(ptr(PointerEvent::Type::Move, {b.right() + 50, b.center().y}));   // drag beyond the end
    CHECK(s.value() == 100);
    send(ptr(PointerEvent::Type::Up, {b.right() + 50, b.center().y}));
    press(key::Left);
    CHECK(s.value() == 95);                 // snapped to the step
    press(key::Home);
    CHECK(s.value() == 0);
    s.set_value(37);
    CHECK(s.value() == 35);
    p.set_fraction(1.5f);
    CHECK(p.fraction() == 1.f);
}

static void test_text_field() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    TextField& tf = shell.content().add<TextField>("", "placeholder");
    relayout();
    std::string last; int activations = 0;
    tf.changed.connect([&](const std::string& s) { last = s; });
    tf.activated.connect([&] { ++activations; });
    click(tf);
    CHECK(tf.has_focus());
    type("hello world");
    CHECK(tf.text() == "hello world" && last == "hello world");
    press(key::BackSpace, MOD_CTRL);          // delete the previous word
    CHECK(tf.text() == "hello ");
    type("नमस्ते");                          // multi-byte UTF-8
    press(key::BackSpace);                    // one code point
    CHECK(tf.text().size() == std::string("hello नमस्त").size());
    press(key::Home);
    press(key::Right, MOD_SHIFT); press(key::Right, MOD_SHIFT);
    CHECK(tf.selected_text() == "he");
    type("J");                              // typing replaces the selection
    CHECK(tf.text().rfind("Jllo ", 0) == 0);
    press(key::letter('a'), MOD_CTRL);
    CHECK(tf.selected_text() == tf.text());
    press(key::Delete);
    CHECK(tf.text().empty());
    press(key::Return);
    CHECK(activations == 1);
    tf.set_text("one two three");
    click(tf, 2);                           // double-click selects a word
    CHECK(!tf.selected_text().empty() && tf.selected_text().find(' ') == std::string::npos);
    click(tf, 3);
    CHECK(tf.selected_text() == "one two three");
}

static void test_spin_and_dropdown() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    SpinButton& sp = shell.content().add<SpinButton>(0, 10, 5, 1);
    Dropdown& dd = shell.content().add<Dropdown>(std::vector<std::string>{"A", "B", "C"}, 0);
    Button& below = shell.content().add<Button>("below");
    relayout();
    CHECK(sp.field().text() == "5");
    sp.field().grab_focus();
    press(key::Up);
    CHECK(sp.value() == 6);
    sp.field().select_all();
    type("42");
    press(key::Return);
    CHECK(sp.value() == 10);                // clamped
    sp.field().select_all();
    type("abc");
    press(key::Return);
    CHECK(sp.value() == 10 && sp.field().text() == "10");   // invalid input is restored

    int changed = -1; dd.changed.connect([&](int i) { changed = i; });
    click(dd);
    CHECK(dd.is_open() && shell.host().has_popup());
    relayout();
    press(key::Down);
    press(key::Down);
    press(key::Return);                       // pick "C" with the keyboard
    CHECK(!dd.is_open() && dd.selected() == 2 && changed == 2);
    CHECK(dd.has_focus());                  // focus returns to the dropdown
    click(dd);
    press(key::Escape);
    CHECK(!dd.is_open() && dd.selected() == 2);
    // An outside click closes the popup and is consumed (the button below is not clicked).
    int below_clicks = 0; below.clicked.connect([&] { ++below_clicks; });
    click(dd);
    relayout();
    click(below);
    CHECK(!dd.is_open() && below_clicks == 0);
    click(below);
    CHECK(below_clicks == 1);
}

static void test_tooltip_scroll_tabs() {
    WindowShell shell(nullptr, "t"); g_shell = &shell;
    ScrollView& sv = shell.content().add<ScrollView>();
    Box& page = sv.set_content<Box>(Orientation::Vertical, 10.f);
    std::vector<Button*> buttons;
    for (int i = 0; i < 40; ++i) buttons.push_back(&page.add<Button>("Button " + std::to_string(i)));
    buttons[0]->set_tooltip("first");
    relayout();
    CHECK(sv.max_offset() > 0);
    shell.host().show_tooltip(buttons[0], center(*buttons[0]));
    relayout();
    shell.host().hide_tooltip();

    ScrollEvent wheel; wheel.pos = center(sv); wheel.dy = 30;
    send(wheel);
    CHECK(sv.offset() > 0);
    // Keyboard focus far down scrolls it into view.
    sv.scroll_to(0, false);
    relayout();
    shell.host().set_focus(buttons[39], true);
    CHECK(sv.offset() == sv.max_offset());

    WindowShell shell2(nullptr, "t"); g_shell = &shell2;
    TabView& tabs = shell2.content().add<TabView>();
    Box& p1 = tabs.add_tab("One");
    Box& p2 = tabs.add_tab("Two");
    p1.add<Button>("in one");
    p2.add<Button>("in two");
    relayout();
    CHECK(tabs.current() == 0 && p1.visible() && !p2.visible());
    int changed = -1; tabs.changed.connect([&](int i) { changed = i; });
    tabs.set_current(1);
    CHECK(changed == 1 && !p1.visible() && p2.visible());

    Badge& badge = shell2.content().add<Badge>("12");
    relayout();
    CHECK(badge.size().height == shell2.theme().badge_height);
}

int main() {
    test_toggles();
    test_slider_progress();
    test_text_field();
    test_spin_and_dropdown();
    test_tooltip_scroll_tabs();
    if (failures) { fprintf(stderr, "%d check(s) failed\n", failures); return 1; }
    printf("test_controls: all checks passed\n");
    return 0;
}
