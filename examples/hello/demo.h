// The demo UI: a gallery of the basic controls, shared by the hello app (on screen) and
// uitk_snapshot (offscreen).
#pragma once
#include <string>

#include "uitk/core/signal.h"
#include "uitk/widgets/button.h"
#include "uitk/widgets/color_swatch.h"
#include "uitk/widgets/dropdown.h"
#include "uitk/widgets/range.h"
#include "uitk/widgets/text_field.h"
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

    // ---- theme (presets are uitk::Theme::presets()) ----
    // The preset with the user's customizations (style, accent, roundness, text size) applied.
    uitk::Theme current_theme() const;
    uitk::Signal<bool> theme_changed;   // apply current_theme(); the flag says whether to cross-fade
    void select_preset(int index);      // emits theme_changed
    void toggle_dark();                 // back to the last-used preset of the other brightness
    // Customizations, as the controls set them (also used by uitk_snapshot).
    void set_accent(int index);         // 0 = the preset's own, 1.. = accent_colors()
    void set_roundness(float scale);
    void set_text_size(float px);       // 0 = the preset's own

    void set_metrics(const Metrics& m);

    uitk::Button* primary_button = nullptr;   // for snapshots
    uitk::ScrollView* scroll = nullptr;

private:
    void sync_controls();   // reflect the settings in the controls without re-emitting
    void changed(bool animate = true);

    int preset_ = 0, style_ = 0, accent_ = 0;
    int last_dark_ = 0, last_light_ = 1;
    float roundness_ = 1, text_size_ = 0;
    bool syncing_ = false;
    uitk::Dropdown* preset_box_ = nullptr;
    uitk::Dropdown* style_box_ = nullptr;
    uitk::ColorSwatchPicker* accents_ = nullptr;
    uitk::Slider* roundness_slider_ = nullptr;
    uitk::Label* roundness_value_ = nullptr;
    uitk::SpinButton* text_size_box_ = nullptr;
    uitk::RadioGroup endings_;
    uitk::Button* theme_button_ = nullptr;
    uitk::Button* env_ = nullptr;
    uitk::Button* metrics_ = nullptr;
    uitk::Label* clicks_ = nullptr;
    int click_count_ = 0;
};

} // namespace hello
