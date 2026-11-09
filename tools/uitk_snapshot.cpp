// Render the demo window offscreen to a PNG (through whichever backend this binary links).
// It uses the same WindowShell + demo code as the real app.
// usage: uitk_snapshot_<cpu|gpu> <out.png> [--dark] [--scale S] [--size WxH]
//            [--hover min|max|close|primary] [--focus] [--maximized] [--inactive]
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uitk/render/renderer.h"
#include <functional>

#include "uitk/widgets/dropdown.h"
#include "uitk/widgets/text_field.h"
#include "uitk/widgets/window_shell.h"

#include "../examples/hello/demo.h"

using namespace uitk;

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <out.png> [--dark] [--scale S] [--size WxH] [--hover min|max|close|primary]"
                        " [--focus] [--maximized] [--inactive]\n", argv[0]);
        return 2;
    }
    const char* out = argv[1];
    bool dark = false, maximized = false, focus = false, inactive = false, adwaita = false;
    bool dropdown = false, tooltip = false;
    float scroll_y = 0;
    const char* type_text = nullptr;
    const char* hover = nullptr;
    float scale = 1;
    Size size{900, 640};
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--dark")) dark = true;
        else if (!strcmp(argv[i], "--maximized")) maximized = true;
        else if (!strcmp(argv[i], "--focus")) focus = true;
        else if (!strcmp(argv[i], "--inactive")) inactive = true;
        else if (!strcmp(argv[i], "--style") && i + 1 < argc) adwaita = !strcmp(argv[++i], "adwaita");
        else if (!strcmp(argv[i], "--dropdown")) dropdown = true;
        else if (!strcmp(argv[i], "--tooltip")) tooltip = true;
        else if (!strcmp(argv[i], "--scroll") && i + 1 < argc) scroll_y = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--type") && i + 1 < argc) type_text = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            int w, h; if (sscanf(argv[++i], "%dx%d", &w, &h) == 2) size = {(float)w, (float)h};
        } else if (!strcmp(argv[i], "--hover") && i + 1 < argc) hover = argv[++i];
        else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; }
    }

    WindowShell shell(nullptr, "uitk hello (snapshot)");
    hello::Demo demo(shell);
    int preset = (adwaita ? 2 : 0) + (dark ? 0 : 1);   // index into Demo::presets()
    shell.set_theme(hello::Demo::presets()[(size_t)preset].second, false);
    demo.show_theme(preset);
    shell.set_maximized(maximized);
    shell.set_active(!inactive);

    // Surface = window + shadow margin (none when maximized), as on screen.
    Insets m = maximized ? Insets{} : WindowFrame::shadow_insets();
    auto r = OffscreenRenderer::create({size.width + m.left + m.right, size.height + m.top + m.bottom}, scale);
    hello::Metrics metrics;
    metrics.platform = "offscreen";
    metrics.backend = r->description();
    metrics.cpu_percent = 3.2f; metrics.memory_mb = 48;   // sample values
    demo.set_metrics(metrics);

    // Interaction states are produced by real events, exactly as on screen.
    shell.host().layout(size);
    if (hover) {
        Button* b = !strcmp(hover, "min") ? &shell.header_bar().minimize_button()
                  : !strcmp(hover, "max") ? &shell.header_bar().maximize_button()
                  : !strcmp(hover, "close") ? &shell.header_bar().close_button()
                  : demo.primary_button;
        PointerEvent move;
        move.type = PointerEvent::Type::Move;
        move.pos = b->to_window(b->local_rect().center());
        shell.handle_event(move);
    }
    if (focus) shell.host().focus_next(false);
    if (scroll_y > 0) { demo.scroll->scroll_to(scroll_y, false); shell.host().layout(size); }
    // Find widgets by type in the demo (first match).
    auto find_first = [&](auto pred) {
        std::function<Widget*(Widget*)> walk = [&](Widget* w) -> Widget* {
            if (pred(w)) return w;
            for (auto& c : w->children()) if (Widget* r = walk(c.get())) return r;
            return nullptr;
        };
        return walk(shell.host().root());
    };
    if (type_text) {
        if (auto* tf = dynamic_cast<TextField*>(find_first([](Widget* w) { return dynamic_cast<TextField*>(w) != nullptr; }))) {
            tf->grab_focus();
            shell.handle_event(TextEvent{type_text});
            KeyEvent shift_left; shift_left.keysym = key::Left; shift_left.mods = MOD_SHIFT;
            for (int k = 0; k < 3; ++k) shell.handle_event(shift_left);   // select the last 3 characters
        }
    }
    if (dropdown) {
        if (auto* dd = dynamic_cast<Dropdown*>(find_first([](Widget* w) { return dynamic_cast<Dropdown*>(w) != nullptr; }))) {
            dd->open();
            shell.host().layout(size);
        }
    }
    if (tooltip && demo.primary_button) {
        shell.host().show_tooltip(demo.primary_button, demo.primary_button->to_window(demo.primary_button->local_rect().center()));
        shell.host().layout(size);
    }

    Canvas* c = r->begin_frame();
    c->translate(m.left, m.top);
    shell.paint(*c, size);
    r->end_frame();

    if (!r->write_png(out)) { fprintf(stderr, "failed to write %s\n", out); return 1; }
    printf("%s: %s\n", out, r->description().c_str());
    return 0;
}
