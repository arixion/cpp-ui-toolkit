// Demo app. Built twice: hello_cpu (links uitk_render_cpu) and hello_gpu (uitk_render_gpu).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "uitk/core/application.h"
#include "uitk/core/perf.h"
#include "uitk/widgets/application_window.h"

#include "demo.h"

#ifndef UITK_HELLO_BACKEND
#define UITK_HELLO_BACKEND "?"
#endif

using namespace uitk;
using Clock = std::chrono::steady_clock;

int main(int argc, char** argv) {
    // --bench N: continuous redraw for N seconds, print average frame rate and CPU, exit.
    double bench_seconds = 0;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--bench") bench_seconds = atof(argv[i + 1]);

    Application app;
    WindowDesc desc;
    desc.title = "uitk hello (" UITK_HELLO_BACKEND ")";
    desc.app_id = "uitk-hello";
    desc.size = {900, 640};
    ApplicationWindow win(app, desc);
    hello::Demo demo(win.shell());

    hello::Metrics metrics;
    metrics.platform = win.window().platform_name();
    metrics.backend = win.renderer().description();
    fprintf(stderr, "hello: platform=%s renderer=%s\n", metrics.platform.c_str(), metrics.backend.c_str());
    demo.set_metrics(metrics);

    // ---- theme: VS Code Dark to start; radio buttons pick a preset, T toggles dark/light ----
    int theme_index = 0;
    auto apply_theme = [&](int i, bool animate) {
        theme_index = i;
        win.set_theme(hello::Demo::presets()[(size_t)i].second, animate);   // every widget cross-fades
        demo.show_theme(i);
    };
    apply_theme(0, false);
    demo.theme_selected.connect([&](int i) { apply_theme(i, true); });
    auto toggle_theme = [&] { apply_theme(theme_index ^ 1, true); };

    // ---- frame statistics ----
    auto fps_start = Clock::now();
    int frames = 0;
    double draw_ms_sum = 0;
    long total_frames = 0;
    win.frame_painted.connect([&](double ms) {
        ++frames; ++total_frames; draw_ms_sum += ms;
        double elapsed = std::chrono::duration<double>(Clock::now() - fps_start).count();
        if (app.loop_mode() == LoopMode::Continuous && elapsed >= 1.0) {
            metrics.fps = (float)(frames / elapsed);
            metrics.frame_ms = (float)(draw_ms_sum / frames);
            fprintf(stderr, "hello: %.1f fps, %.2f ms/frame draw, CPU %.1f%%\n", metrics.fps, metrics.frame_ms, metrics.cpu_percent);
            demo.set_metrics(metrics);
            fps_start = Clock::now(); frames = 0; draw_ms_sum = 0;
        }
    });

    auto set_continuous = [&](bool on) {
        app.set_loop_mode(on ? LoopMode::Continuous : LoopMode::OnDemand);
        metrics.continuous = on;
        if (!on) metrics.fps = 0;
        fps_start = Clock::now(); frames = 0; draw_ms_sum = 0;
        demo.set_metrics(metrics);
        win.window().request_redraw();
    };

    // ---- shortcuts (keys no widget used) ----
    win.key_pressed.connect([&](const KeyEvent& k) {
        if (k.repeat) return;
        if (k.keysym == key::letter('t')) toggle_theme();
        else if (k.keysym == key::letter('f')) set_continuous(app.loop_mode() != LoopMode::Continuous);
    });

    // ---- process CPU / memory, once per second ----
    CpuMeter cpu;
    app.add_timer(std::chrono::milliseconds(1000), [&] {
        metrics.cpu_percent = cpu.sample();
        metrics.memory_mb = (float)resident_memory_bytes() / (1024.f * 1024.f);
        demo.set_metrics(metrics);
    }, /*repeat=*/true);

    if (bench_seconds > 0) {
        set_continuous(true);
        auto bench_cpu = std::make_shared<CpuMeter>();
        auto bench_start = Clock::now();
        app.add_timer(std::chrono::milliseconds((long)(bench_seconds * 1000)), [&, bench_cpu] {
            double secs = std::chrono::duration<double>(Clock::now() - bench_start).count();
            printf("bench: platform=%s renderer=%s frames=%ld avg_fps=%.1f cpu=%.1f%% rss=%.0fMB\n",
                   metrics.platform.c_str(), metrics.backend.c_str(), total_frames, total_frames / secs,
                   bench_cpu->sample(), (double)resident_memory_bytes() / (1024.0 * 1024.0));
            app.quit();
        });
    }
    return app.run();
}
