// TextField: single-line text input. SpinButton: numeric input with -/+ (TextField + Buttons).
//
// Editing: typing, Backspace/Delete (Ctrl: by word), Left/Right/Home/End (Shift extends the
// selection, Ctrl moves by word), Ctrl+A, mouse click/drag selection, double-click selects a
// word, triple-click selects all. Text is UTF-8; the caret moves by code point.
// (Clipboard copy/paste needs platform clipboard support, which is not implemented yet.)
#pragma once
#include <string>

#include "uitk/core/application.h"
#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/button.h"

namespace uitk {

class TextField : public Widget {
public:
    explicit TextField(std::string text = {}, std::string placeholder = {});
    ~TextField() override;

    Signal<const std::string&> changed;   // after every edit
    Signal<> activated;                    // Enter
    Signal<> editing_finished;             // Enter or focus lost

    void set_text(std::string text);       // caret to the end, selection cleared
    const std::string& text() const { return text_; }
    void set_placeholder(std::string p) { placeholder_ = std::move(p); invalidate(); }
    // No background or border: for fields embedded in a composite that draws the frame
    // (SearchField, command center).
    void set_frameless(bool f) { if (f != frameless_) { frameless_ = f; invalidate(); } }
    bool frameless() const { return frameless_; }
    void select_all();
    void set_selection(size_t anchor, size_t caret);
    size_t caret() const { return caret_; }
    bool has_selection() const { return anchor_ != caret_; }
    std::string selected_text() const;

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    MouseCursor cursor() const override { return MouseCursor::Text; }
    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;   // keeps the caret visible after resizes
    void paint(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;
    bool on_text(const TextEvent& e) override;

protected:
    void on_state_changed(uint32_t old_state) override;

private:
    TextLayout layout_text(const Theme& t) const;
    Rect text_box() const;                       // padding-inset content area
    size_t index_at_x(float local_x) const;
    void move_caret(size_t pos, bool extend);
    void replace_selection(const std::string& with);
    void erase(bool forward, bool word);
    size_t prev_char(size_t i) const;
    size_t next_char(size_t i) const;
    size_t prev_word(size_t i) const;
    size_t next_word(size_t i) const;
    void ensure_caret_visible();
    void restart_blink();
    void stop_blink();

    std::string text_, placeholder_;
    size_t caret_ = 0, anchor_ = 0;
    float scroll_x_ = 0;
    bool dragging_ = false;
    bool caret_on_ = true;
    bool frameless_ = false;
    TimerId blink_timer_ = 0;
    StateLayer layer_;
};

class SpinButton : public Box {
public:
    SpinButton(double min = 0, double max = 100, double value = 0, double step = 1, int decimals = 0);

    Signal<double> changed;
    void set_value(double v);
    double value() const { return value_; }
    TextField& field() { return *field_; }

    bool on_key(const KeyEvent& e) override;   // Up/Down/PageUp/PageDown from the field

private:
    void commit_text();
    void show();

    double min_, max_, value_, step_;
    int decimals_;
    TextField* field_;
};

} // namespace uitk
