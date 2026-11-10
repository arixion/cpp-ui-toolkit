// Theme: the single source of design tokens (colours, metrics, typography, motion, and the
// window-chrome style) for all widgets. Widgets never hard-code colours or sizes; they ask the
// theme, so a whole design language (VS Code, Adwaita, ...) is just a different Theme value,
// and theme switches cross-fade everywhere at once.
#pragma once
#include <string>
#include <vector>

#include "uitk/core/animation.h"
#include "uitk/core/color.h"
#include "uitk/render/canvas.h"

namespace uitk {

enum class TextRole {
    Body,      // default text
    Dim,       // secondary text (descriptions, sources)
    Caption,   // small chrome text (status bar)
    Chrome,    // UI chrome text (menus, panel headers, title bar)
    Heading,   // large page heading
    Button,    // text inside buttons
};

// Named theme colours. Widgets that store a token (not a Color) resolve it at paint time, so
// they follow theme changes and cross-fades without being updated.
enum class ColorToken {
    Foreground, ForegroundDim, Chrome, Accent, SelectionFg, SelectionFgDim, OnAccent,
    Error,   // error messages and failure icons
};

// Colours of one kind of control. Hover/press tint the background with `overlay` at the
// theme's hover/press opacities (one rule for every control: the StateLayer).
struct ControlColors {
    Color background;
    Color foreground;
    Color overlay;
    friend bool operator==(const ControlColors&, const ControlColors&) = default;
};

enum class WindowControlsStyle {
    Circular,   // round buttons with symbolic icons (GNOME / Adwaita)
    Flat,       // flat full-height buttons with thin line glyphs (VS Code)
};

enum class TitleStyle {
    Plain,           // centred title text
    CommandCenter,   // title inside a centred input-like box (VS Code command center)
};

// The handful of colours that define a colour scheme. Theme::with_palette() expands one into
// every theme colour, so a new scheme (or a user's custom one) is just these values.
struct ThemePalette {
    Color background;       // window, title bar, status bar, content
    Color surface;          // cards, notifications
    Color input;            // text fields, command center
    Color border;           // panel / card borders
    Color input_border;     // input outlines
    Color foreground;       // primary text and icons
    Color foreground_dim;   // secondary text
    Color chrome;           // menus, title bar, panel headers, status bar text
    Color accent;           // filled accents: primary button, checks, switches, sliders
    Color on_accent;        // text and marks drawn on `accent`
    Color accent_text;      // accent used as text on `background`: links, focus ring
    Color button;           // secondary button fill
    Color selection_bg;     // selected list row
    Color selection_fg;
    Color danger;           // destructive button, errors
    Color on_danger;
};

struct Theme {
    std::string name;

    // ---- surfaces & text ----
    Color window_bg;         // frame, title bar, status bar, editor/content background
    Color surface_bg;        // cards, notifications, popups
    Color input_bg;          // text fields, command center
    Color border;            // card / panel borders
    Color input_border;      // input outlines
    Color foreground;        // primary text and icons
    Color foreground_dim;    // secondary text
    Color chrome_fg;         // menus, title bar, panel headers, status bar
    Color accent;            // links, focus, highlights
    Color focus_ring;
    Color shadow;            // window shadow colour
    Color selection_bg;      // selected list row
    Color selection_fg;
    Color selection_fg_dim;

    // ---- controls ----
    ControlColors button;              // standard (secondary) buttons
    ControlColors button_primary;      // the main action
    ControlColors button_destructive;  // delete / discard
    ControlColors button_flat;         // toolbar / icon buttons, list rows (transparent at rest)
    ControlColors window_control;      // minimize / maximize / close

    // ---- component colours (filled in by derive() from the colours above; overridable) ----
    Color text_selection;    // selected text background
    Color caret;
    Color check_fill;        // checked box / selected radio / switch-on track / slider fill
    Color check_mark;        // mark drawn on check_fill
    Color control_bg;        // unchecked box, switch-off track, text field
    Color control_border;    // outlines of boxes, radios, fields
    Color track;             // empty slider / progress track
    Color scrollbar_thumb;
    Color popup_bg;          // dropdown lists, menus, tooltips
    Color popup_border;
    Color tooltip_fg;
    Color badge_bg, badge_fg;
    Color link;
    ControlColors toggle_on; // ToggleButton while checked

    float hover_opacity    = 0.12f;    // overlay strength while hovered
    float press_opacity    = 0.10f;    // additional overlay while pressed
    float disabled_opacity = 0.50f;    // multiplier for disabled controls

    // ---- metrics (logical px) ----
    float radius               = 4;    // buttons, inputs
    float toolbar_radius       = 5;    // icon buttons
    float card_radius          = 6;    // notifications, cards
    float list_radius          = 4;    // list row selection
    float window_radius        = 8;
    float control_height       = 26;
    float control_padding_x    = 12;
    float toolbar_button_size  = 24;
    float list_row_height      = 26;
    float spacing              = 6;
    float icon_size            = 16;
    float focus_ring_width     = 1;
    float focus_ring_offset    = 1;
    float header_height        = 35;
    float window_button_width  = 34;   // Flat style (Circular uses window_button_height square)
    float window_button_height = 35;
    float window_icon_size     = 16;
    float status_bar_height    = 22;
    float command_center_width = 480;
    float command_center_height = 24;
    float check_size           = 16;   // check box / radio indicator
    float switch_width         = 32;
    float switch_height        = 18;
    float slider_thumb         = 14;
    float track_thickness      = 4;
    float scrollbar_width      = 10;
    float tab_height           = 32;
    float popup_radius         = 6;
    float badge_height         = 18;

    WindowControlsStyle window_controls = WindowControlsStyle::Flat;
    TitleStyle title_style = TitleStyle::CommandCenter;

    // ---- typography ----
    std::vector<std::string> font_families = {"Adwaita Sans", "Inter", "Cantarell", "Noto Sans", "DejaVu Sans", "Segoe UI", "sans-serif"};
    float font_size         = 13;
    float font_size_small   = 12;
    float font_size_heading = 26;
    int   button_weight     = 400;
    int   title_weight      = 400;
    int   heading_weight    = 600;

    // ---- motion ----
    Transition hover_in  {Millis(100), Easing::ease_out_quad()};
    Transition hover_out {Millis(150), Easing::ease_out_quad()};
    Transition press_in  {Millis(50),  Easing::ease_out_quad()};
    Transition press_out {Millis(150), Easing::ease_out_quad()};
    Transition focus     {Millis(120), Easing::ease_out_quad()};
    Transition backdrop  {Millis(200), Easing::ease_in_out_cubic()};   // window (in)active
    Transition theme     {Millis(250), Easing::ease_in_out_cubic()};   // theme cross-fade

    // Fills the component colours from the base colours (every preset calls this).
    void derive();

    // ---- customization (each returns a modified copy; chain them) ----
    // Replace every colour from a palette; metrics, chrome style, fonts and motion are kept.
    Theme with_palette(const ThemePalette& p) const;
    // Recolour the accent (primary button, checks, switches, sliders, links, focus ring). Text on
    // the accent and the link colour are picked for contrast against it / the window.
    Theme with_accent(Color accent) const;
    // Metrics, window-chrome style, fonts and motion of `look`; colours of this theme.
    Theme with_look_of(const Theme& look) const;
    // Scale every corner radius (0 = square, 1 = as designed, 2 = twice as round).
    Theme with_roundness(float scale) const;
    // Body text size in px; small and heading sizes keep their proportions.
    Theme with_font_size(float px) const;
    bool is_dark() const { return window_bg.is_dark(); }

    // ---- presets ----
    static Theme vscode_dark();    // matches the reference VS Code setup (black, teal accent)
    static Theme vscode_light();   // the vscode_dark palette re-toned for a light window
    static Theme adwaita_light();
    static Theme adwaita_dark();
    static Theme nord();           // Nord: Polar Night background, Frost accents, Aurora red
    static Theme abyss();          // VS Code Abyss: deep navy, blue text, ocean-blue accent
    static Theme absolute_dark();  // pure black, neutral greys, white accent
    static Theme absolute_light(); // pure white, neutral greys, black accent

    struct Preset { const char* name; Theme (*make)(); };
    static const std::vector<Preset>& presets();   // every built-in preset, dark/light pairs adjacent
    static Theme light() { return vscode_light(); }   // toolkit defaults
    static Theme dark() { return vscode_dark(); }

    // Text style for a role, ready to use with TextLayout / Label.
    TextStyle text(TextRole role) const;
    Color text_color(TextRole role) const;
    Color color(ColorToken token) const;

    // Compares the visual tokens (colours, metrics, styles); motion and fonts are not compared.
    friend bool operator==(const Theme& a, const Theme& b);
};

ControlColors lerp(const ControlColors& a, const ControlColors& b, float t);
// Colours and opacities interpolate; metrics, styles and other fields come from `b`.
// Enables Animated<Theme> (cross-fades between presets).
Theme lerp(const Theme& a, const Theme& b, float t);

} // namespace uitk
