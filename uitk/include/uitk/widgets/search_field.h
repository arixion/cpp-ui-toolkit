// SearchField: a text field with a search icon, a clear button and optional inline toggles
// (VS Code find widget: "Match Case", "Regular Expression").
//
//   auto& s = box.add<SearchField>("Search files");
//   ToggleButton& case_ = s.add_option(icons::symbolic("format-text-uppercase"), "Match Case");
//   s.set_debounce(150);
//   s.search_changed.connect([](const std::string& q) { ... });   // after typing pauses
//   s.activated.connect([](const std::string& q) { ... });        // Enter (remembered in history)
//
// Escape clears the text (and bubbles when it is already empty). With history enabled, Up/Down
// recall earlier searches.
#pragma once
#include <string>
#include <vector>

#include "uitk/core/application.h"
#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"

namespace uitk {

class Button;
class Icon;
class TextField;
class ToggleButton;

class SearchField : public Box {
public:
    explicit SearchField(std::string placeholder = "Search");
    ~SearchField() override;

    TextField& field() { return *field_; }
    const std::string& text() const;
    void set_text(std::string text);   // emits search_changed immediately
    void clear() { set_text({}); }
    ToggleButton& add_option(Image icon, std::string tooltip);

    void set_debounce(int milliseconds) { debounce_ms_ = milliseconds; }   // 0: every keystroke
    void set_history_enabled(bool on) { history_enabled_ = on; }
    const std::vector<std::string>& history() const { return history_; }

    Signal<const std::string&> search_changed;
    Signal<const std::string&> activated;

    void paint(PaintContext& ctx) override;
    bool on_key(const KeyEvent& e) override;

private:
    void text_edited(const std::string& text);
    void flush();
    void remember(const std::string& text);

    Icon* icon_;
    TextField* field_;
    Button* clear_;
    Box* options_;
    int debounce_ms_ = 0;
    TimerId timer_ = 0;
    bool history_enabled_ = false;
    std::vector<std::string> history_;
    long history_pos_ = -1;   // -1: not browsing
};

} // namespace uitk
