#include "demo.h"

#include <cmath>
#include <cstdio>

#include "uitk/widgets/box.h"
#include "uitk/widgets/color_swatch.h"
#include "uitk/widgets/dropdown.h"
#include "uitk/widgets/frame.h"
#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/icon.h"
#include "uitk/widgets/icons.h"
#include "uitk/widgets/range.h"
#include "uitk/widgets/status_bar.h"
#include "uitk/widgets/text_field.h"

namespace hello {

using namespace uitk;

namespace {
// Accent choices after "the preset's own" (GNOME's accent colours).
const std::vector<Color>& accent_colors() {
    static const std::vector<Color> c = {
        Color::hex(0x3584e4), Color::hex(0x2190a4), Color::hex(0x3a944a), Color::hex(0xc88800), Color::hex(0xed5b00),
        Color::hex(0xe62d42), Color::hex(0xd56199), Color::hex(0x9141ac), Color::hex(0x6f8396)};
    return c;
}
const char* const kAccentNames[] = {"Theme default", "Blue", "Teal", "Green", "Yellow", "Orange",
                                    "Red", "Pink", "Purple", "Slate"};
const char* const kStyles[] = {"Theme default", "VS Code", "Adwaita"};

Theme preset_theme(int i) { return Theme::presets()[(size_t)i].make(); }

// A titled section: heading row + body box.
Box& section(Box& page, const char* title, const char* note = nullptr) {
    Box& sec = page.add<Box>(Orientation::Vertical, 10.f);
    Box& head = sec.add<Box>(Orientation::Horizontal, 10.f);
    head.add<Label>(title, TextRole::Chrome).set_align(Align::Start, Align::Center);
    if (note) head.add<Label>(note, TextRole::Dim).set_align(Align::Start, Align::Center);
    return sec;
}
Box& row(Box& parent, float spacing = 8.f) {
    Box& r = parent.add<Box>(Orientation::Horizontal, spacing);
    return r;
}
Label& caption(Box& r, const char* text, float width = 0) {
    Label& l = r.add<Label>(text, TextRole::Body);
    l.set_align(Align::Start, Align::Center);
    if (width > 0) l.set_min_size({width, 0});
    return l;
}
} // namespace

Demo::Demo(WindowShell& shell) {
    auto track = [this](Button& b) {
        b.clicked.connect([this, &b] {
            ++click_count_;
            std::string name = b.label().empty() ? "icon button" : b.label();
            clicks_->set_text("Clicked “" + name + "” · " + std::to_string(click_count_) +
                              (click_count_ == 1 ? " click" : " clicks") + " total");
        });
        return &b;
    };

    // ---- title bar ----
    theme_button_ = &shell.header_bar().start().add<Button>(icons::symbolic("weather-clear"), ButtonStyle::Flat);
    theme_button_->set_tooltip("Toggle dark / light (T)");
    theme_button_->clicked.connect([this] { toggle_dark(); });

    // ---- scrollable gallery ----
    scroll = &shell.content().add<ScrollView>();
    Box& page = scroll->set_content<Box>(Orientation::Vertical, 26.f);
    page.set_padding(Insets{28, 22, 28, 28});

    Box& title = row(page, 12.f);
    title.add<Label>("Controls", TextRole::Heading).set_align(Align::Start, Align::End);
    title.add<Label>("basic widgets of uitk", TextRole::Dim).set_align(Align::Start, Align::End);

    // Appearance: preset + customizations
    {
        Box& sec = section(page, "Appearance", "pick a theme, then customize it");
        Box& r1 = row(sec, 12.f);
        caption(r1, "Theme", 80);
        std::vector<std::string> names;
        for (const auto& p : Theme::presets()) names.emplace_back(p.name);
        preset_box_ = &r1.add<Dropdown>(names, 0);
        preset_box_->set_fixed_size({170, 0});
        preset_box_->changed.connect([this](int i) { if (!syncing_) select_preset(i); });
        caption(r1, "Style");
        style_box_ = &r1.add<Dropdown>(std::vector<std::string>(std::begin(kStyles), std::end(kStyles)), 0);
        style_box_->set_fixed_size({150, 0});
        style_box_->set_tooltip("Metrics and window chrome, independent of the colours");
        style_box_->changed.connect([this](int i) { if (!syncing_) { style_ = i; changed(); } });

        Box& r2 = row(sec, 12.f);
        caption(r2, "Accent", 80);
        accents_ = &r2.add<ColorSwatchPicker>(std::vector<Color>(accent_colors().size() + 1), 0);
        for (int i = 0; i < accents_->count(); ++i) accents_->swatch(i).set_tooltip(kAccentNames[i]);
        accents_->changed.connect([this](int i) { set_accent(i); });

        Box& r3 = row(sec, 12.f);
        caption(r3, "Roundness", 80);
        roundness_slider_ = &r3.add<Slider>(0, 200, 100, 25);
        roundness_slider_->set_fixed_size({180, 0});
        roundness_value_ = &caption(r3, "100%", 44);
        roundness_slider_->changed.connect([this](float v) { if (!syncing_) set_roundness(v / 100); });
        caption(r3, "Text size");
        text_size_box_ = &r3.add<SpinButton>(10, 18, 13, 1);
        text_size_box_->changed.connect([this](double v) { if (!syncing_) set_text_size((float)v); });
        Button& reset = r3.add<Button>("Reset", ButtonStyle::Flat);
        reset.set_tooltip("Back to the preset as designed");
        reset.clicked.connect([this] {
            style_ = 0; accent_ = 0; roundness_ = 1; text_size_ = 0;
            changed();
        });
    }

    // Buttons
    {
        Box& sec = section(page, "Buttons");
        Box& r1 = row(sec);
        primary_button = track(r1.add<Button>("Save", ButtonStyle::Primary));
        primary_button->set_tooltip("Primary action");
        track(r1.add<Button>("Cancel"));
        track(r1.add<Button>("Delete", ButtonStyle::Destructive));
        track(r1.add<Button>("Flat", ButtonStyle::Flat));
        Button& open = r1.add<Button>("Open Folder");
        open.set_icon(icons::symbolic("folder"));
        track(open);
        r1.add<Button>("Disabled").set_enabled(false);

        Box& r2 = row(sec);
        ToggleButton& bold = r2.add<ToggleButton>("Match Case");
        bold.set_tooltip("Toggle button: stays on while checked");
        ToggleButton& wrap = r2.add<ToggleButton>(icons::symbolic("view-wrapped"));
        wrap.set_tooltip("Icon toggle");
        wrap.set_checked(true);
        for (const char* name : {"document-new", "document-save", "edit-find", "view-refresh"}) {
            Button& b = r2.add<Button>(icons::symbolic(name), ButtonStyle::Flat);
            b.set_tooltip(name);
            track(b);
        }
        r2.add<Widget>().set_min_size({8, 1});
        Button& link = r2.add<Button>("Learn more", ButtonStyle::Link);
        link.set_tooltip("Link button");
        track(link);
        r2.add<Badge>("3");
        r2.add<Badge>("New");
    }

    // Selection
    {
        Box& sec = section(page, "Selection", "check boxes, radio buttons, switches");
        Box& cols = row(sec, 48.f);
        Box& c1 = cols.add<Box>(Orientation::Vertical, 8.f);
        c1.add<CheckBox>("Auto save").set_checked(true);
        c1.add<CheckBox>("Format on save");
        c1.add<CheckBox>("Mixed (indeterminate)").set_indeterminate(true);
        c1.add<CheckBox>("Disabled").set_enabled(false);

        Box& c2 = cols.add<Box>(Orientation::Vertical, 8.f);
        for (const char* e : {"LF (Unix)", "CRLF (Windows)", "CR (classic Mac)"}) c2.add<RadioButton>(e, endings_);
        c2.add<RadioButton>("Disabled", endings_).set_enabled(false);
        endings_.select(0);

        Box& c3 = cols.add<Box>(Orientation::Vertical, 8.f);
        c3.add<Switch>("Wi-Fi").set_checked(true);
        c3.add<Switch>("Bluetooth");
        Switch& off = c3.add<Switch>("Airplane mode (disabled)");
        off.set_enabled(false);
    }

    // Text input
    {
        Box& sec = section(page, "Text input");
        Box& r1 = row(sec);
        TextField& search = r1.add<TextField>("", "Search files…");
        search.set_fixed_size({240, 0});
        TextField& name = r1.add<TextField>("Hello, world — नमस्ते");
        name.set_fixed_size({240, 0});
        TextField& off = r1.add<TextField>("Read only");
        off.set_fixed_size({140, 0});
        off.set_enabled(false);
        Box& r2 = row(sec, 16.f);
        r2.add<Label>("Tab size", TextRole::Body).set_align(Align::Start, Align::Center);
        r2.add<SpinButton>(1, 16, 4, 1);
        r2.add<Label>("Language", TextRole::Body).set_align(Align::Start, Align::Center);
        r2.add<Dropdown>(std::vector<std::string>{"C++", "Rust", "Python", "TypeScript", "Go"}, 0);
    }

    // Range & progress
    {
        Box& sec = section(page, "Range & progress");
        Box& r1 = row(sec, 16.f);
        Slider& slider = r1.add<Slider>(0, 100, 40, 1);
        slider.set_fixed_size({260, 0});
        Label& value = r1.add<Label>("40", TextRole::Body);
        value.set_align(Align::Start, Align::Center);
        value.set_min_size({32, 0});
        ProgressBar& bar = r1.add<ProgressBar>(0.4f);
        bar.set_fixed_size({220, 0});
        slider.changed.connect([&value, &bar](float v) {
            value.set_text(std::to_string((int)std::lround(v)));
            bar.set_fraction(v / 100);
        });

        // Busy indicators animate every frame, so they start stopped (idle CPU stays ~0).
        Box& r2 = row(sec, 16.f);
        ProgressBar& busy = r2.add<ProgressBar>();
        busy.set_fixed_size({260, 0});
        Spinner& spin = r2.add<Spinner>();
        ToggleButton& run = r2.add<ToggleButton>("Start activity");
        run.toggled.connect([&busy, &spin, &run](bool on) {
            busy.set_indeterminate(on);
            if (on) spin.start(); else spin.stop();
            run.set_label(on ? "Stop activity" : "Start activity");
        });
    }

    // Tabs
    {
        Box& sec = section(page, "Tabs");
        TabView& tabs = sec.add<TabView>();
        tabs.set_fixed_size({640, 0});
        Box& general = tabs.add_tab("General", Orientation::Vertical, 8.f);
        general.set_padding(Insets{4, 12, 4, 4});
        general.add<Label>("Each tab hosts any widget tree; the indicator slides between tabs.").set_wrap(true);
        general.add<CheckBox>("Confirm before closing");
        Box& editor = tabs.add_tab("Editor", Orientation::Vertical, 8.f);
        editor.set_padding(Insets{4, 12, 4, 4});
        editor.add<Switch>("Word wrap").set_checked(true);
        editor.add<TextField>("", "Font family").set_fixed_size({240, 0});
        Box& appearance = tabs.add_tab("Appearance", Orientation::Vertical, 8.f);
        appearance.set_padding(Insets{4, 12, 4, 4});
        appearance.add<Label>("Themes are picked and customized in “Appearance” above.", TextRole::Dim);
    }

    // Display
    {
        Box& sec = section(page, "Display", "labels, icons, separator, tooltips");
        Box& r1 = row(sec, 16.f);
        r1.add<Label>("Heading", TextRole::Heading).set_align(Align::Start, Align::End);
        r1.add<Label>("Body text", TextRole::Body).set_align(Align::Start, Align::End);
        r1.add<Label>("Dim text", TextRole::Dim).set_align(Align::Start, Align::End);
        r1.add<Label>("Caption", TextRole::Caption).set_align(Align::Start, Align::End);
        sec.add<Separator>();
        Box& r2 = row(sec, 12.f);
        for (Glyph g : {Glyph::Info, Glyph::Warning, Glyph::Error, Glyph::Check, Glyph::ChevronRight, Glyph::More})
            r2.add<Icon>(glyph(g));
        for (const char* name : {"folder", "user-home", "starred", "input-gaming"}) r2.add<Icon>(icons::symbolic(name));
        r2.add<Label>("Hover a button to see its tooltip.", TextRole::Dim).set_align(Align::Start, Align::Center);
        clicks_ = &sec.add<Label>("Click a control, or use Tab / arrows / Space.", TextRole::Dim);
    }

    // ---- status bar ----
    StatusBar& status = shell.content().add<StatusBar>();
    status.add_start("main", icons::symbolic("view-refresh"));
    status.add_start("0", glyph(Glyph::Error));
    status.add_start("0", glyph(Glyph::Warning));
    env_ = &status.add_start("");
    metrics_ = &status.add_end("CPU –");
    status.add_end("T theme · F fps");
}

Theme Demo::current_theme() const {
    Theme t = preset_theme(preset_);
    if (style_ == 1) t = t.with_look_of(Theme::vscode_dark());
    if (style_ == 2) t = t.with_look_of(Theme::adwaita_dark());
    if (accent_ > 0) t = t.with_accent(accent_colors()[(size_t)accent_ - 1]);
    if (roundness_ != 1) t = t.with_roundness(roundness_);
    if (text_size_ > 0) t = t.with_font_size(text_size_);
    return t;
}

void Demo::select_preset(int index) {
    preset_ = index;
    (preset_theme(index).is_dark() ? last_dark_ : last_light_) = index;
    changed();
}

void Demo::toggle_dark() { select_preset(preset_theme(preset_).is_dark() ? last_light_ : last_dark_); }

void Demo::set_accent(int index) { accent_ = index; changed(); }
void Demo::set_roundness(float scale) { roundness_ = scale; changed(false); }   // follows the slider live
void Demo::set_text_size(float px) { text_size_ = px; changed(); }

void Demo::changed(bool animate) {
    sync_controls();
    theme_changed.emit(animate);
}

void Demo::sync_controls() {
    syncing_ = true;
    Theme preset = preset_theme(preset_);
    preset_box_->set_selected(preset_);
    style_box_->set_selected(style_);
    std::vector<Color> swatches = {preset.button_primary.background};   // first swatch: the preset's own
    swatches.insert(swatches.end(), accent_colors().begin(), accent_colors().end());
    accents_->set_colors(swatches);
    accents_->set_selected(accent_);
    roundness_slider_->set_value(roundness_ * 100);
    roundness_value_->set_text(std::to_string((int)std::lround(roundness_ * 100)) + "%");
    text_size_box_->set_value(text_size_ > 0 ? text_size_ : preset.font_size);
    syncing_ = false;
    theme_button_->set_icon(icons::symbolic(preset.is_dark() ? "weather-clear" : "weather-clear-night"));
}

void Demo::set_metrics(const Metrics& m) {
    env_->set_label(m.platform + " · " + m.backend);
    char buf[160];
    int n = m.cpu_percent >= 0 ? snprintf(buf, sizeof buf, "CPU %.1f%%", m.cpu_percent)
                               : snprintf(buf, sizeof buf, "CPU –");
    if (m.memory_mb > 0) n += snprintf(buf + n, sizeof buf - n, " · RAM %.0f MB", m.memory_mb);
    if (m.continuous && m.fps > 0) snprintf(buf + n, sizeof buf - n, " · %.0f fps · %.1f ms", m.fps, m.frame_ms);
    metrics_->set_label(buf);
}

} // namespace hello
