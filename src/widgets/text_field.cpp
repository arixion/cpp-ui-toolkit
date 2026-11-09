#include "uitk/widgets/text_field.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/label.h"

namespace uitk {

namespace {
constexpr float kPadX = 8;
constexpr uint32_t kKpEnter = 0xff8d;
constexpr int kBlinkMs = 530;
bool is_word_byte(unsigned char c) { return c >= 0x80 || std::isalnum(c) || c == '_'; }
}

TextField::TextField(std::string text, std::string placeholder)
    : text_(std::move(text)), placeholder_(std::move(placeholder)) {
    set_align(Align::Fill, Align::Center);
    caret_ = anchor_ = text_.size();
}

TextField::~TextField() { stop_blink(); }

// ---------------------------------------------------------------------------- text model

size_t TextField::prev_char(size_t i) const {
    if (i == 0) return 0;
    --i;
    while (i > 0 && ((unsigned char)text_[i] & 0xC0) == 0x80) --i;   // skip UTF-8 continuation bytes
    return i;
}

size_t TextField::next_char(size_t i) const {
    if (i >= text_.size()) return text_.size();
    ++i;
    while (i < text_.size() && ((unsigned char)text_[i] & 0xC0) == 0x80) ++i;
    return i;
}

size_t TextField::prev_word(size_t i) const {
    while (i > 0 && !is_word_byte((unsigned char)text_[prev_char(i)])) i = prev_char(i);
    while (i > 0 && is_word_byte((unsigned char)text_[prev_char(i)])) i = prev_char(i);
    return i;
}

size_t TextField::next_word(size_t i) const {
    while (i < text_.size() && !is_word_byte((unsigned char)text_[i])) i = next_char(i);
    while (i < text_.size() && is_word_byte((unsigned char)text_[i])) i = next_char(i);
    return i;
}

void TextField::set_text(std::string text) {
    if (text == text_) return;
    text_ = std::move(text);
    caret_ = anchor_ = text_.size();
    ensure_caret_visible();
    invalidate();
    changed.emit(text_);
}

void TextField::select_all() { set_selection(0, text_.size()); }

void TextField::set_selection(size_t anchor, size_t caret) {
    anchor_ = std::min(anchor, text_.size());
    caret_ = std::min(caret, text_.size());
    ensure_caret_visible();
    restart_blink();
    invalidate();
}

std::string TextField::selected_text() const {
    size_t a = std::min(anchor_, caret_), b = std::max(anchor_, caret_);
    return text_.substr(a, b - a);
}

void TextField::move_caret(size_t pos, bool extend) {
    caret_ = std::min(pos, text_.size());
    if (!extend) anchor_ = caret_;
    ensure_caret_visible();
    restart_blink();
    invalidate();
}

void TextField::replace_selection(const std::string& with) {
    size_t a = std::min(anchor_, caret_), b = std::max(anchor_, caret_);
    text_.replace(a, b - a, with);
    caret_ = anchor_ = a + with.size();
    ensure_caret_visible();
    restart_blink();
    invalidate();
    changed.emit(text_);
}

void TextField::erase(bool forward, bool word) {
    if (!has_selection()) {
        if (forward) caret_ = word ? next_word(caret_) : next_char(caret_);
        else caret_ = word ? prev_word(caret_) : prev_char(caret_);
        if (caret_ == anchor_) return;
    }
    replace_selection("");
}

// ---------------------------------------------------------------------------- geometry

TextLayout TextField::layout_text(const Theme& t) const {
    TextStyle s = Label::style_for(t, TextRole::Body, TextAlign::Left, false);
    return TextLayout(text_, s);   // unbounded width: one line
}

Rect TextField::text_box() const { return local_rect().inset(frameless_ ? 2.f : kPadX, 0); }

size_t TextField::index_at_x(float local_x) const {
    return layout_text(theme()).index_at(local_x - text_box().x + scroll_x_, 5);
}

void TextField::layout(const Theme&) { ensure_caret_visible(); }

void TextField::ensure_caret_visible() {
    float pad = text_box().x;
    if (size().width <= 2 * pad) { scroll_x_ = 0; return; }   // not laid out yet
    float avail = std::max(0.f, size().width - 2 * pad - 1);
    float x = layout_text(theme()).caret_x(caret_);
    if (x - scroll_x_ > avail) scroll_x_ = x - avail;
    if (x - scroll_x_ < 0) scroll_x_ = x;
    float total = layout_text(theme()).size().width;
    scroll_x_ = std::clamp(scroll_x_, 0.f, std::max(0.f, total - avail));
}

Size TextField::measure(float, const Theme& t) { return {200, t.control_height}; }

// ---------------------------------------------------------------------------- paint

void TextField::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    StateLayer::Sample s = layer_.sample(ctx.clock);
    Rect r = local_rect();
    bool focused = has_focus();
    if (!frameless_) {
        ctx.canvas.draw_rrect(r, t.radius, Paint::fill(StateLayer::content(t.control_bg, s, t)));
        // Border: accent while focused (fades in), field outline otherwise.
        Color border = lerp(t.control_border, t.focus_ring, focused ? 1.f : s.focus_ring);
        ctx.canvas.draw_rrect(r.inset(0.5f, 0.5f), t.radius - 0.5f, Paint::stroke(StateLayer::content(border, s, t), 1));
    }

    Rect box = text_box();
    ctx.canvas.save();
    ctx.canvas.clip_rect(box);
    TextStyle style = Label::style_for(t, TextRole::Body, TextAlign::Left, false);
    if (text_.empty()) {
        if (!placeholder_.empty()) {
            style.color = StateLayer::content(t.foreground_dim, s, t);
            TextLayout ph(placeholder_, style);
            ctx.canvas.draw_text(ph, {box.x, std::round((r.height - ph.size().height) / 2)});
        }
    }
    style.color = StateLayer::content(t.foreground, s, t);
    TextLayout tl(text_, style);
    float ty = std::round((r.height - (text_.empty() ? std::ceil(t.font_size * 1.2f) : tl.size().height)) / 2);
    float ox = box.x - scroll_x_;
    if (focused && has_selection())
        for (Rect sel : tl.range_rects(std::min(anchor_, caret_), std::max(anchor_, caret_)))
            ctx.canvas.draw_rect({ox + sel.x, ty + sel.y, sel.width, sel.height}, Paint::fill(t.text_selection));
    if (!text_.empty()) ctx.canvas.draw_text(tl, {ox, ty});
    if (focused && caret_on_) {
        float cx = std::round(ox + tl.caret_x(caret_)) + 0.5f;
        float h = std::ceil(t.font_size * 1.3f);
        float cy = std::round((r.height - h) / 2);
        ctx.canvas.draw_line({cx, cy}, {cx, cy + h}, Paint::stroke(t.caret, 1));
    }
    ctx.canvas.restore();
}

// ---------------------------------------------------------------------------- focus / blink

void TextField::on_state_changed(uint32_t old) {
    layer_.update(state(), theme());
    bool was = old & STATE_FOCUSED, now = state() & STATE_FOCUSED;
    if (now && !was) restart_blink();
    if (was && !now) {
        stop_blink();
        dragging_ = false;
        editing_finished.emit();
    }
}

void TextField::restart_blink() {
    caret_on_ = true;
    Application* app = Application::current();
    if (!app || !has_focus()) return;
    if (blink_timer_) app->cancel_timer(blink_timer_);
    blink_timer_ = app->add_timer(Millis(kBlinkMs), [this] { caret_on_ = !caret_on_; invalidate(); }, true);
}

void TextField::stop_blink() {
    if (blink_timer_) if (auto* app = Application::current()) app->cancel_timer(blink_timer_);
    blink_timer_ = 0;
    caret_on_ = true;
    invalidate();
}

// ---------------------------------------------------------------------------- input

bool TextField::on_pointer(const PointerEvent& e) {
    if (e.button != PointerButton::Left && e.type != PointerEvent::Type::Move) return e.type == PointerEvent::Type::Down;
    switch (e.type) {
        case PointerEvent::Type::Down: {
            size_t i = index_at_x(e.pos.x);
            if (e.click_count >= 3) { select_all(); }
            else if (e.click_count == 2) {   // the word under the pointer
                size_t a = i, b = i;
                while (a > 0 && is_word_byte((unsigned char)text_[prev_char(a)])) a = prev_char(a);
                while (b < text_.size() && is_word_byte((unsigned char)text_[b])) b = next_char(b);
                set_selection(a, b);
            }
            else move_caret(i, e.mods & MOD_SHIFT);
            dragging_ = e.click_count < 2;
            return true;
        }
        case PointerEvent::Type::Move:
            if (dragging_) move_caret(index_at_x(e.pos.x), true);
            return dragging_;
        case PointerEvent::Type::Up:
            dragging_ = false;
            return true;
        default:
            return false;
    }
}

bool TextField::on_key(const KeyEvent& e) {
    if (!e.pressed || !enabled()) return false;
    bool shift = e.mods & MOD_SHIFT, ctrl = e.mods & MOD_CTRL;
    switch (e.keysym) {
        case key::Left:
            if (has_selection() && !shift) move_caret(std::min(anchor_, caret_), false);
            else move_caret(ctrl ? prev_word(caret_) : prev_char(caret_), shift);
            return true;
        case key::Right:
            if (has_selection() && !shift) move_caret(std::max(anchor_, caret_), false);
            else move_caret(ctrl ? next_word(caret_) : next_char(caret_), shift);
            return true;
        case key::Home: move_caret(0, shift); return true;
        case key::End: move_caret(text_.size(), shift); return true;
        case key::BackSpace: erase(false, ctrl); return true;
        case key::Delete: erase(true, ctrl); return true;
        case key::Return: case kKpEnter: activated.emit(); editing_finished.emit(); return true;
    }
    if (ctrl && (e.keysym == key::letter('a'))) { select_all(); return true; }
    return false;   // Tab, Up/Down etc. bubble (focus traversal, SpinButton)
}

bool TextField::on_text(const TextEvent& e) {
    if (!enabled() || e.utf8.empty()) return false;
    replace_selection(e.utf8);
    return true;
}

// ---------------------------------------------------------------------------- SpinButton

SpinButton::SpinButton(double min, double max, double value, double step, int decimals)
    : Box(Orientation::Horizontal, 2.f), min_(min), max_(max), value_(min), step_(step), decimals_(decimals) {
    set_align(Align::Start, Align::Center);
    field_ = &add<TextField>();
    field_->set_fixed_size({80, 0});
    Button& minus = add<Button>(glyph(Glyph::Minus), ButtonStyle::Flat);
    Button& plus = add<Button>(glyph(Glyph::Plus), ButtonStyle::Flat);
    minus.set_focusable(false);
    plus.set_focusable(false);
    minus.clicked.connect([this] { set_value(value_ - step_); });
    plus.clicked.connect([this] { set_value(value_ + step_); });
    field_->editing_finished.connect([this] { commit_text(); });
    value_ = std::clamp(value, min_, max_);
    show();
}

void SpinButton::show() {
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", decimals_, value_);
    field_->set_text(buf);
}

void SpinButton::set_value(double v) {
    v = std::clamp(v, min_, max_);
    double scale = std::pow(10.0, decimals_);
    v = std::round(v * scale) / scale;
    bool changed_now = v != value_;
    value_ = v;
    show();
    if (changed_now) changed.emit(value_);
}

void SpinButton::commit_text() {
    char* end = nullptr;
    double v = strtod(field_->text().c_str(), &end);
    if (end && end != field_->text().c_str()) set_value(v);
    else show();   // not a number: restore
}

bool SpinButton::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    switch (e.keysym) {
        case key::Up:       commit_text(); set_value(value_ + step_); return true;
        case key::Down:     commit_text(); set_value(value_ - step_); return true;
        case key::PageUp:   commit_text(); set_value(value_ + 10 * step_); return true;
        case key::PageDown: commit_text(); set_value(value_ - 10 * step_); return true;
    }
    return false;
}

} // namespace uitk
