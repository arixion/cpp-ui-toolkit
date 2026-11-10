#include "uitk/widgets/theme.h"

#include <algorithm>
#include <cmath>

namespace uitk {

// ---------------------------------------------------------------------------- derived tokens

void Theme::derive() {
    auto mix = [this](float t) { return lerp(window_bg, foreground, t); };
    text_selection  = accent.with_opacity(0.35f);
    caret           = foreground;
    check_fill      = button_primary.background;
    check_mark      = button_primary.foreground;
    control_bg      = input_bg;
    control_border  = mix(0.35f);
    track           = mix(0.18f);
    scrollbar_thumb = foreground.with_opacity(0.28f);
    popup_bg        = mix(0.06f);
    popup_border    = mix(0.18f);
    tooltip_fg      = foreground;
    badge_bg        = button_primary.background;
    badge_fg        = button_primary.foreground;
    link            = accent;
    toggle_on       = {mix(0.16f), foreground, foreground};
}

// ---------------------------------------------------------------------------- customization

Theme Theme::with_palette(const ThemePalette& p) const {
    Theme t = *this;
    t.window_bg        = p.background;
    t.surface_bg       = p.surface;
    t.input_bg         = p.input;
    t.border           = p.border;
    t.input_border     = p.input_border;
    t.foreground       = p.foreground;
    t.foreground_dim   = p.foreground_dim;
    t.chrome_fg        = p.chrome;
    t.accent           = p.accent_text;
    t.focus_ring       = p.accent_text.with_alpha(focus_ring.a ? focus_ring.a : 255);
    t.shadow           = Color::hex(0x000000);
    t.selection_bg     = p.selection_bg;
    t.selection_fg     = p.selection_fg;
    t.selection_fg_dim = lerp(p.selection_fg, p.selection_bg, 0.4f);
    t.button             = {p.button, p.foreground, p.foreground};
    t.button_primary     = {p.accent, p.on_accent, p.on_accent};
    t.button_destructive = {p.danger, p.on_danger, p.on_danger};
    t.button_flat        = {Color::transparent(), p.foreground, p.foreground};
    t.window_control = window_controls == WindowControlsStyle::Circular
                           ? ControlColors{p.foreground.with_opacity(0.10f), p.foreground, p.foreground}
                           : ControlColors{Color::transparent(), p.chrome, p.foreground};
    t.derive();
    return t;
}

Theme Theme::with_accent(Color c) const {
    Theme t = *this;
    const Color white = Color::hex(0xffffff), black = Color::hex(0x000000);
    Color on = contrast_ratio(c, white) >= 3.f ? white : black;
    // As text on the window the accent needs 4.5:1; pull it toward the foreground until it has it.
    Color text = c;
    for (int i = 0; i < 20 && contrast_ratio(text, window_bg) < 4.5f; ++i) text = lerp(text, foreground, 0.1f);
    t.accent = text;
    t.focus_ring = text.with_alpha(focus_ring.a);
    t.button_primary = {c, on, on};
    t.derive();
    return t;
}

Theme Theme::with_look_of(const Theme& look) const {
    Theme t = lerp(*this, look, 0.f);   // colours (t = 0) from this, everything else from `look`
    t.name = name;
    return t;
}

Theme Theme::with_roundness(float s) const {
    Theme t = *this;
    s = std::max(0.f, s);
    for (float* r : {&t.radius, &t.toolbar_radius, &t.card_radius, &t.list_radius, &t.popup_radius, &t.window_radius})
        *r = std::round(*r * s);
    return t;
}

Theme Theme::with_font_size(float px) const {
    Theme t = *this;
    float f = px / font_size;
    t.font_size = px;
    t.font_size_small = std::round(font_size_small * f);
    t.font_size_heading = std::round(font_size_heading * f);
    return t;
}

const std::vector<Theme::Preset>& Theme::presets() {
    static const std::vector<Preset> p = {
        {"VS Code Dark", vscode_dark},     {"VS Code Light", vscode_light},
        {"Adwaita Dark", adwaita_dark},    {"Adwaita Light", adwaita_light},
        {"Absolute Dark", absolute_dark},  {"Absolute Light", absolute_light},
        {"Nord", nord},                    {"Abyss", abyss},
    };
    return p;
}

// ---------------------------------------------------------------------------- palette presets
// These use the default (VS Code) metrics and chrome; combine with with_look_of() for others.

Theme Theme::nord() {
    // nordtheme.com: Polar Night nord0-3, Snow Storm nord4-6, Frost nord7-10, Aurora nord11-15.
    ThemePalette p;
    p.background     = Color::hex(0x2e3440);   // nord0
    p.surface        = Color::hex(0x2e3440);
    p.input          = Color::hex(0x3b4252);   // nord1
    p.border         = Color::hex(0x3b4252);
    p.input_border   = Color::hex(0x4c566a);   // nord3
    p.foreground     = Color::hex(0xd8dee9);   // nord4
    p.foreground_dim = Color::hex(0x939cad);   // nord3 / nord4 midpoint, readable on nord0
    p.chrome         = Color::hex(0xb3bbc9);
    p.accent         = Color::hex(0x88c0d0);   // nord8
    p.on_accent      = Color::hex(0x2e3440);
    p.accent_text    = Color::hex(0x88c0d0);
    p.button         = Color::hex(0x434c5e);   // nord2
    p.selection_bg   = Color::hex(0x434c5e);
    p.selection_fg   = Color::hex(0xeceff4);   // nord6
    p.danger         = Color::hex(0xbf616a);   // nord11
    p.on_danger      = Color::hex(0xeceff4);
    Theme t = Theme{}.with_palette(p);
    t.name = "Nord";
    return t;
}

Theme Theme::abyss() {
    // VS Code's built-in Abyss: navy editor, periwinkle text, ocean-blue badges, gold cursor.
    ThemePalette p;
    p.background     = Color::hex(0x000c18);
    p.surface        = Color::hex(0x060621);
    p.input          = Color::hex(0x181f2f);
    p.border         = Color::hex(0x10192c);
    p.input_border   = Color::hex(0x2b3c5d);
    p.foreground     = Color::hex(0x8aa4e0);   // Abyss text (#6688cc) lifted for small UI text
    p.foreground_dim = Color::hex(0x6580bd);
    p.chrome         = Color::hex(0x6688cc);
    p.accent         = Color::hex(0x0063a5);   // badges / buttons
    p.on_accent      = Color::hex(0xffffff);
    p.accent_text    = Color::hex(0x4f9dff);
    p.button         = Color::hex(0x1b2a47);
    p.selection_bg   = Color::hex(0x08286b);
    p.selection_fg   = Color::hex(0xddbb88);   // Abyss cursor gold
    p.danger         = Color::hex(0xa22d44);
    p.on_danger      = Color::hex(0xffffff);
    Theme t = Theme{}.with_palette(p);
    t.name = "Abyss";
    return t;
}

Theme Theme::absolute_dark() {
    ThemePalette p;
    p.background     = Color::hex(0x000000);
    p.surface        = Color::hex(0x000000);
    p.input          = Color::hex(0x0a0a0a);
    p.border         = Color::hex(0x1f1f1f);
    p.input_border   = Color::hex(0x333333);
    p.foreground     = Color::hex(0xf2f2f2);
    p.foreground_dim = Color::hex(0x8c8c8c);
    p.chrome         = Color::hex(0xa6a6a6);
    p.accent         = Color::hex(0xffffff);
    p.on_accent      = Color::hex(0x000000);
    p.accent_text    = Color::hex(0xffffff);
    p.button         = Color::hex(0x1f1f1f);
    p.selection_bg   = Color::hex(0x262626);
    p.selection_fg   = Color::hex(0xffffff);
    p.danger         = Color::hex(0xff453a);
    p.on_danger      = Color::hex(0x000000);
    Theme t = Theme{}.with_palette(p);
    t.name = "Absolute Dark";
    return t;
}

Theme Theme::absolute_light() {
    ThemePalette p;
    p.background     = Color::hex(0xffffff);
    p.surface        = Color::hex(0xffffff);
    p.input          = Color::hex(0xffffff);
    p.border         = Color::hex(0xe6e6e6);
    p.input_border   = Color::hex(0xcccccc);
    p.foreground     = Color::hex(0x0d0d0d);
    p.foreground_dim = Color::hex(0x6b6b6b);
    p.chrome         = Color::hex(0x595959);
    p.accent         = Color::hex(0x000000);
    p.on_accent      = Color::hex(0xffffff);
    p.accent_text    = Color::hex(0x000000);
    p.button         = Color::hex(0xebebeb);
    p.selection_bg   = Color::hex(0xebebeb);
    p.selection_fg   = Color::hex(0x000000);
    p.danger         = Color::hex(0xd70015);
    p.on_danger      = Color::hex(0xffffff);
    Theme t = Theme{};
    t.hover_opacity = 0.07f;
    t = t.with_palette(p);
    t.name = "Absolute Light";
    return t;
}

// ---------------------------------------------------------------------------- VS Code

Theme Theme::vscode_dark() {
    // Colours sampled from the reference VS Code screenshot.
    Theme t;
    t.name             = "VS Code Dark";
    t.window_bg        = Color::hex(0x000000);
    t.surface_bg       = Color::hex(0x000000);
    t.input_bg         = Color::hex(0x101214);
    t.border           = Color::hex(0x1f1f1f);
    t.input_border     = Color::hex(0x2b3034);
    t.foreground       = Color::hex(0xbfd4e1);
    t.foreground_dim   = Color::hex(0x78838a);
    t.chrome_fg        = Color::hex(0x5f6a70);
    t.accent           = Color::hex(0xa9dadc);
    t.focus_ring       = Color::hex(0xa9dadc);
    t.shadow           = Color::hex(0x000000);
    t.selection_bg     = Color::hex(0x222222);
    t.selection_fg     = Color::hex(0xf0f3bd);
    t.selection_fg_dim = Color::hex(0xb4b791);

    t.button             = {Color::hex(0x222222), Color::hex(0xbfd4e1), Color::hex(0xbfd4e1)};
    t.button_primary     = {Color::hex(0xa9dadc), Color::hex(0x000000), Color::hex(0x000000)};
    t.button_destructive = {Color::hex(0xf14c4c), Color::hex(0x000000), Color::hex(0x000000)};
    t.button_flat        = {Color::transparent(), Color::hex(0xbfd4e1), Color::hex(0xbfd4e1)};
    t.window_control     = {Color::transparent(), Color::hex(0x5f6a70), Color::hex(0xbfd4e1)};
    t.derive();
    return t;
}

Theme Theme::vscode_light() {
    // The VS Code Dark palette (blue-grey text, teal accent, pale-yellow selection, slate greys,
    // red) re-toned for a light window: same hues, lightness flipped where contrast needs it.
    Theme t;
    t.name             = "VS Code Light";
    t.window_bg        = Color::hex(0xf6f9fb);   // bfd4e1 lifted almost to white
    t.surface_bg       = Color::hex(0xf6f9fb);
    t.input_bg         = Color::hex(0xffffff);
    t.border           = Color::hex(0xdce5eb);
    t.input_border     = Color::hex(0xc2ced5);
    t.foreground       = Color::hex(0x1d2b34);   // bfd4e1 hue, deep
    t.foreground_dim   = Color::hex(0x5f6a70);   // dark theme's chrome grey
    t.chrome_fg        = Color::hex(0x5f6a70);
    t.accent           = Color::hex(0x2b7c80);   // a9dadc hue, deep enough for text on white
    t.focus_ring       = Color::hex(0x2b7c80);
    t.shadow           = Color::hex(0x1d2b34);
    t.selection_bg     = Color::hex(0xf0f3bd);   // the dark theme's selection yellow
    t.selection_fg     = Color::hex(0x1d2b34);
    t.selection_fg_dim = Color::hex(0x5c5f30);

    t.button             = {Color::hex(0xe2eaef), Color::hex(0x1d2b34), Color::hex(0x1d2b34)};
    t.button_primary     = {Color::hex(0x8fcdd0), Color::hex(0x0d1f20), Color::hex(0x0d1f20)};
    t.button_destructive = {Color::hex(0xd23c3c), Color::hex(0xffffff), Color::hex(0xffffff)};
    t.button_flat        = {Color::transparent(), Color::hex(0x1d2b34), Color::hex(0x1d2b34)};
    t.window_control     = {Color::transparent(), Color::hex(0x5f6a70), Color::hex(0x1d2b34)};
    t.hover_opacity = 0.07f;
    t.press_opacity = 0.10f;
    t.derive();
    return t;
}

// ---------------------------------------------------------------------------- Adwaita

namespace {
Theme adwaita_base() {
    Theme t;
    t.radius = 6;
    t.toolbar_radius = 6;
    t.card_radius = 12;
    t.list_radius = 6;
    t.window_radius = 10;
    t.control_height = 28;
    t.toolbar_button_size = 28;
    t.header_height = 32;
    t.window_button_width = 22;
    t.window_button_height = 22;
    t.window_icon_size = 14;
    t.focus_ring_width = 2;
    t.window_controls = WindowControlsStyle::Circular;
    t.title_style = TitleStyle::Plain;
    t.font_size_heading = 18;
    t.button_weight = 500;
    t.title_weight = 600;
    t.hover_opacity = 0.05f;
    t.press_opacity = 0.15f;
    return t;
}
} // namespace

Theme Theme::adwaita_light() {
    Theme t = adwaita_base();
    t.name = "Adwaita Light";
    Color fg = Color::rgb(36, 31, 49);
    t.window_bg = t.surface_bg = Color::rgb(255, 255, 255);
    t.input_bg = Color::rgb(245, 245, 245);
    t.border = Color::rgb(222, 222, 222);
    t.input_border = Color::rgb(210, 210, 210);
    t.foreground = t.chrome_fg = fg;
    t.foreground_dim = Color::rgb(120, 120, 128);
    t.accent = Color::hex(0x3584e4);
    t.focus_ring = Color::hex(0x3584e4).with_opacity(0.5f);
    t.shadow = Color::rgb(0, 0, 0);
    t.selection_bg = Color::hex(0x3584e4).with_opacity(0.15f);
    t.selection_fg = fg;
    t.selection_fg_dim = t.foreground_dim;
    t.button = {fg.with_opacity(0.10f), fg, fg};
    t.button_primary = {Color::hex(0x3584e4), Color::rgb(255, 255, 255), Color::rgb(255, 255, 255)};
    t.button_destructive = {Color::hex(0xe01b24), Color::rgb(255, 255, 255), Color::rgb(255, 255, 255)};
    t.button_flat = {Color::transparent(), fg, fg};
    t.window_control = {fg.with_opacity(0.10f), fg, fg};
    t.derive();
    t.check_size = 18;
    t.switch_width = 40;
    t.switch_height = 22;
    t.slider_thumb = 18;
    return t;
}

Theme Theme::adwaita_dark() {
    Theme t = adwaita_base();
    t.name = "Adwaita Dark";
    Color fg = Color::rgb(226, 226, 226);
    t.window_bg = t.surface_bg = Color::rgb(0, 0, 0);
    t.input_bg = Color::rgb(30, 30, 30);
    t.border = Color::rgb(46, 46, 46);
    t.input_border = Color::rgb(60, 60, 60);
    t.foreground = t.chrome_fg = fg;
    t.foreground_dim = Color::rgb(140, 140, 140);
    t.accent = Color::hex(0x78aeed);
    t.focus_ring = Color::hex(0x78aeed).with_opacity(0.5f);
    t.shadow = Color::rgb(0, 0, 0);
    t.selection_bg = Color::hex(0x3584e4).with_opacity(0.25f);
    t.selection_fg = fg;
    t.selection_fg_dim = t.foreground_dim;
    t.button = {fg.with_opacity(0.12f), fg, fg};
    t.button_primary = {Color::hex(0x3584e4), Color::rgb(255, 255, 255), Color::rgb(255, 255, 255)};
    t.button_destructive = {Color::hex(0xc01c28), Color::rgb(255, 255, 255), Color::rgb(255, 255, 255)};
    t.button_flat = {Color::transparent(), fg, fg};
    t.window_control = {fg.with_opacity(0.12f), fg, fg};
    t.derive();
    t.check_size = 18;
    t.switch_width = 40;
    t.switch_height = 22;
    t.slider_thumb = 18;
    return t;
}

// ---------------------------------------------------------------------------- text

Color Theme::text_color(TextRole role) const {
    switch (role) {
        case TextRole::Dim:     return foreground_dim;
        case TextRole::Caption:
        case TextRole::Chrome:  return chrome_fg;
        default:                return foreground;
    }
}

Color Theme::color(ColorToken token) const {
    switch (token) {
        case ColorToken::Foreground:     return foreground;
        case ColorToken::ForegroundDim:  return foreground_dim;
        case ColorToken::Chrome:         return chrome_fg;
        case ColorToken::Accent:         return accent;
        case ColorToken::SelectionFg:    return selection_fg;
        case ColorToken::SelectionFgDim: return selection_fg_dim;
        case ColorToken::OnAccent:       return button_primary.foreground;
        case ColorToken::Error:          return button_destructive.background;
    }
    return foreground;
}

TextStyle Theme::text(TextRole role) const {
    TextStyle s;
    s.families = font_families;
    s.color = text_color(role);
    s.size = font_size;
    switch (role) {
        case TextRole::Caption: s.size = font_size_small; break;
        case TextRole::Chrome:  s.weight = title_weight; break;
        case TextRole::Heading: s.size = font_size_heading; s.weight = heading_weight; break;
        case TextRole::Button:  s.weight = button_weight; break;
        default: break;
    }
    return s;
}

// ---------------------------------------------------------------------------- compare / lerp

bool operator==(const Theme& a, const Theme& b) {
    return a.window_bg == b.window_bg && a.surface_bg == b.surface_bg && a.input_bg == b.input_bg &&
           a.border == b.border && a.input_border == b.input_border && a.foreground == b.foreground &&
           a.foreground_dim == b.foreground_dim && a.chrome_fg == b.chrome_fg && a.accent == b.accent &&
           a.focus_ring == b.focus_ring && a.shadow == b.shadow && a.selection_bg == b.selection_bg &&
           a.selection_fg == b.selection_fg && a.selection_fg_dim == b.selection_fg_dim &&
           a.button == b.button && a.button_primary == b.button_primary &&
           a.button_destructive == b.button_destructive && a.button_flat == b.button_flat &&
           a.window_control == b.window_control && a.hover_opacity == b.hover_opacity &&
           a.press_opacity == b.press_opacity && a.disabled_opacity == b.disabled_opacity &&
           a.radius == b.radius && a.control_height == b.control_height && a.font_size == b.font_size &&
           a.header_height == b.header_height && a.window_controls == b.window_controls &&
           a.title_style == b.title_style && a.window_radius == b.window_radius;
}

ControlColors lerp(const ControlColors& a, const ControlColors& b, float t) {
    return {lerp(a.background, b.background, t), lerp(a.foreground, b.foreground, t), lerp(a.overlay, b.overlay, t)};
}

Theme lerp(const Theme& a, const Theme& b, float t) {
    Theme r = b;   // metrics, styles, fonts and motion from the destination
    auto L = [t](Color x, Color y) { return lerp(x, y, t); };
    r.window_bg = L(a.window_bg, b.window_bg);
    r.surface_bg = L(a.surface_bg, b.surface_bg);
    r.input_bg = L(a.input_bg, b.input_bg);
    r.border = L(a.border, b.border);
    r.input_border = L(a.input_border, b.input_border);
    r.foreground = L(a.foreground, b.foreground);
    r.foreground_dim = L(a.foreground_dim, b.foreground_dim);
    r.chrome_fg = L(a.chrome_fg, b.chrome_fg);
    r.accent = L(a.accent, b.accent);
    r.focus_ring = L(a.focus_ring, b.focus_ring);
    r.shadow = L(a.shadow, b.shadow);
    r.selection_bg = L(a.selection_bg, b.selection_bg);
    r.selection_fg = L(a.selection_fg, b.selection_fg);
    r.selection_fg_dim = L(a.selection_fg_dim, b.selection_fg_dim);
    r.button = lerp(a.button, b.button, t);
    r.button_primary = lerp(a.button_primary, b.button_primary, t);
    r.button_destructive = lerp(a.button_destructive, b.button_destructive, t);
    r.button_flat = lerp(a.button_flat, b.button_flat, t);
    r.window_control = lerp(a.window_control, b.window_control, t);
    r.text_selection = L(a.text_selection, b.text_selection);
    r.caret = L(a.caret, b.caret);
    r.check_fill = L(a.check_fill, b.check_fill);
    r.check_mark = L(a.check_mark, b.check_mark);
    r.control_bg = L(a.control_bg, b.control_bg);
    r.control_border = L(a.control_border, b.control_border);
    r.track = L(a.track, b.track);
    r.scrollbar_thumb = L(a.scrollbar_thumb, b.scrollbar_thumb);
    r.popup_bg = L(a.popup_bg, b.popup_bg);
    r.popup_border = L(a.popup_border, b.popup_border);
    r.tooltip_fg = L(a.tooltip_fg, b.tooltip_fg);
    r.badge_bg = L(a.badge_bg, b.badge_bg);
    r.badge_fg = L(a.badge_fg, b.badge_fg);
    r.link = L(a.link, b.link);
    r.toggle_on = lerp(a.toggle_on, b.toggle_on, t);
    r.hover_opacity = lerp(a.hover_opacity, b.hover_opacity, t);
    r.press_opacity = lerp(a.press_opacity, b.press_opacity, t);
    r.disabled_opacity = lerp(a.disabled_opacity, b.disabled_opacity, t);
    return r;
}

} // namespace uitk
