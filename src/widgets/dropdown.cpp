#include "uitk/widgets/dropdown.h"

#include <algorithm>
#include <cmath>

#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/widget_host.h"

namespace uitk {

namespace {
constexpr uint32_t kSpace = 0x20, kKpEnter = 0xff8d;
constexpr float kCheckColumn = 26, kTextPadRight = 16;
}

// ---------------------------------------------------------------------------- MenuList

MenuList::MenuList(std::vector<std::string> items, int checked)
    : items_(std::move(items)), checked_(checked), highlighted_(checked >= 0 ? checked : 0) {}

Size MenuList::measure(float, const Theme& t) {
    float w = 0;
    TextStyle s = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
    for (auto& it : items_) w = std::max(w, TextLayout(it, s).size().width);
    return {std::ceil(w) + kCheckColumn + kTextPadRight, items_.size() * row_height(t) + 2 * kPad};
}

int MenuList::item_at(float y) const {
    int i = (int)std::floor((y - kPad) / row_height(theme()));
    return (i >= 0 && i < (int)items_.size()) ? i : -1;
}

void MenuList::activate(int index) {
    if (index < 0 || index >= (int)items_.size()) return;
    WidgetHost* h = host();
    activated.emit(index);
    if (h) h->close_popup(this);   // `this` is destroyed after this call
}

void MenuList::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    Rect r = local_rect();
    ctx.canvas.draw_rrect(r, t.popup_radius, Paint::fill(t.popup_bg));
    ctx.canvas.draw_rrect(r.inset(0.5f, 0.5f), t.popup_radius - 0.5f, Paint::stroke(t.popup_border, 1));
    float rh = row_height(t);
    for (int i = 0; i < (int)items_.size(); ++i) {
        Rect row{kPad, kPad + i * rh, r.width - 2 * kPad, rh};
        bool hi = i == highlighted_;
        if (hi) ctx.canvas.draw_rrect(row, t.radius, Paint::fill(t.selection_bg));
        Color fg = hi ? t.selection_fg : t.foreground;
        if (i == checked_) ctx.canvas.draw_image(glyph(Glyph::Check), {row.x + 5, row.y + std::round((rh - 16) / 2), 16, 16}, fg);
        TextStyle s = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
        s.color = fg;
        TextLayout tl(items_[(size_t)i], s, row.width - kCheckColumn);
        ctx.canvas.draw_text(tl, {row.x + kCheckColumn - kPad, row.y + std::round((rh - tl.size().height) / 2)});
    }
}

bool MenuList::on_pointer(const PointerEvent& e) {
    int i = item_at(e.pos.y);
    if (e.type == PointerEvent::Type::Move && i >= 0 && i != highlighted_) { highlighted_ = i; invalidate(); }
    if (e.type == PointerEvent::Type::Up && e.button == PointerButton::Left && i >= 0) activate(i);
    return true;
}

bool MenuList::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    int n = (int)items_.size();
    switch (e.keysym) {
        case key::Up:   highlighted_ = std::max(0, highlighted_ - 1); invalidate(); return true;
        case key::Down: highlighted_ = std::min(n - 1, highlighted_ + 1); invalidate(); return true;
        case key::Home: highlighted_ = 0; invalidate(); return true;
        case key::End:  highlighted_ = n - 1; invalidate(); return true;
        case key::Return: case kKpEnter: case kSpace: activate(highlighted_); return true;
    }
    return e.keysym != key::Tab;   // keep keys inside the popup (except Tab)
}

// ---------------------------------------------------------------------------- Dropdown

Dropdown::Dropdown(std::vector<std::string> items, int selected) : items_(std::move(items)), selected_(selected) {
    set_align(Align::Start, Align::Center);
    if (selected_ >= (int)items_.size()) selected_ = items_.empty() ? -1 : 0;
}

void Dropdown::set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    if (selected_ >= (int)items_.size()) selected_ = items_.empty() ? -1 : 0;
    invalidate_layout();
}

void Dropdown::set_selected(int index) {
    if (index < 0 || index >= (int)items_.size() || index == selected_) return;
    selected_ = index;
    invalidate();
    changed.emit(index);
}

const std::string& Dropdown::selected_text() const {
    static const std::string empty;
    return selected_ >= 0 ? items_[(size_t)selected_] : empty;
}

void Dropdown::open() {
    WidgetHost* h = host();
    if (!h || popup_ || items_.empty()) return;
    WidgetHost::PopupOptions opts;
    opts.match_anchor_width = true;
    opts.on_close = [this] { popup_ = nullptr; on_state_changed(state()); invalidate(); };
    Rect anchor{to_window({0, 0}).x, to_window({0, 0}).y, size().width, size().height};
    MenuList& list = h->emplace_popup<MenuList>(anchor, std::move(opts), items_, selected_);
    list.activated.connect([this](int i) { set_selected(i); });
    popup_ = &list;
    on_state_changed(state());
}

Size Dropdown::measure(float, const Theme& t) {
    float w = 0;
    TextStyle s = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
    for (auto& it : items_) w = std::max(w, TextLayout(it, s).size().width);
    return {std::max(120.f, std::ceil(w) + 2 * t.control_padding_x + 20), t.control_height};
}

void Dropdown::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    StateLayer::Sample s = layer_.sample(ctx.clock);
    Rect r = local_rect();
    ctx.canvas.draw_rrect(r, t.radius, Paint::fill(StateLayer::surface(t.control_bg, t.foreground, s, t)));
    Color border = lerp(t.control_border, t.focus_ring, popup_ ? 1.f : 0.f);
    ctx.canvas.draw_rrect(r.inset(0.5f, 0.5f), t.radius - 0.5f, Paint::stroke(StateLayer::content(border, s, t), 1));
    Color fg = StateLayer::content(t.foreground, s, t);
    TextStyle ts = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
    ts.color = fg;
    TextLayout tl(selected_text(), ts, std::max(0.f, r.width - 2 * t.control_padding_x - 20));
    ctx.canvas.draw_text(tl, {t.control_padding_x, std::round((r.height - tl.size().height) / 2)});
    ctx.canvas.draw_image(glyph(Glyph::ChevronDown), {r.right() - 24, std::round((r.height - 16) / 2), 16, 16}, fg);
}

void Dropdown::paint_over(PaintContext& ctx) {
    StateLayer::paint_focus_ring(ctx.canvas, local_rect(), ctx.theme.radius, layer_.sample(ctx.clock), ctx.theme);
}

bool Dropdown::on_pointer(const PointerEvent& e) {
    if (e.button != PointerButton::Left) return e.type == PointerEvent::Type::Down;
    if (e.type == PointerEvent::Type::Down && enabled()) open();
    return true;
}

bool Dropdown::on_key(const KeyEvent& e) {
    if (!e.pressed || !enabled()) return false;
    switch (e.keysym) {
        case kSpace: case key::Return: case kKpEnter: open(); return true;
        case key::Up:   if (e.mods & MOD_ALT) return false; set_selected(std::max(0, selected_ - 1)); return true;
        case key::Down:
            if (e.mods & MOD_ALT) { open(); return true; }
            set_selected(std::min((int)items_.size() - 1, selected_ + 1));
            return true;
    }
    return false;
}

} // namespace uitk
