// The demo UI: a gallery of the basic controls, shared by the hello app (on screen) and
// uitk_snapshot (offscreen).
#pragma once
#include <string>

#include "uitk/core/signal.h"
#include "uitk/widgets/button.h"
#include "uitk/widgets/containers.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/toggles.h"
#include "uitk/widgets/window_shell.h"

namespace hello {

struct Metrics {
    std::string platform, backend;
    bool  continuous = false;
    float fps = 0, frame_ms = 0;
    float cpu_percent = -1, memory_mb = 0;
};

class Demo {
public:
    explicit Demo(uitk::WindowShell& shell);

    // Theme presets offered by the demo (index into presets()).
    static const std::vector<std::pair<std::string, uitk::Theme>>& presets();
    uitk::Signal<int> theme_selected;   // user picked a preset (radio buttons / header button)
    void show_theme(int index);         // reflect the current theme without re-emitting

    void set_metrics(const Metrics& m);

    uitk::Button* primary_button = nullptr;   // for snapshots
    uitk::ScrollView* scroll = nullptr;

private:
    uitk::RadioGroup theme_group_;
    bool syncing_ = false;
    int theme_index_ = 0;
    uitk::Button* theme_button_ = nullptr;
    uitk::Button* env_ = nullptr;
    uitk::Button* metrics_ = nullptr;
    uitk::Label* clicks_ = nullptr;
    int click_count_ = 0;
};

} // namespace hello
