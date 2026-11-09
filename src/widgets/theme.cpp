#include "uitk/widgets/theme.h"

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
    // VS Code "Light Modern" palette on a white window.
    Theme t;
    t.name             = "VS Code Light";
    t.window_bg        = Color::hex(0xffffff);
    t.surface_bg       = Color::hex(0xffffff);
    t.input_bg         = Color::hex(0xf8f8f8);
    t.border           = Color::hex(0xe5e5e5);
    t.input_border     = Color::hex(0xcecece);
    t.foreground       = Color::hex(0x3b3b3b);
    t.foreground_dim   = Color::hex(0x767676);
    t.chrome_fg        = Color::hex(0x616161);
    t.accent           = Color::hex(0x005fb8);
    t.focus_ring       = Color::hex(0x005fb8);
    t.shadow           = Color::hex(0x000000);
    t.selection_bg     = Color::hex(0xe4e6f1);
    t.selection_fg     = Color::hex(0x1f1f1f);
    t.selection_fg_dim = Color::hex(0x616161);

    t.button             = {Color::hex(0xe5e5e5), Color::hex(0x3b3b3b), Color::hex(0x000000)};
    t.button_primary     = {Color::hex(0x005fb8), Color::hex(0xffffff), Color::hex(0xffffff)};
    t.button_destructive = {Color::hex(0xc42b1c), Color::hex(0xffffff), Color::hex(0xffffff)};
    t.button_flat        = {Color::transparent(), Color::hex(0x3b3b3b), Color::hex(0x000000)};
    t.window_control     = {Color::transparent(), Color::hex(0x616161), Color::hex(0x000000)};
    t.hover_opacity = 0.07f;
    t.press_opacity = 0.07f;
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
