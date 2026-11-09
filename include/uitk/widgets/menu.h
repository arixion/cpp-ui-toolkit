// Menus: a menu model, popup menus with submenus, context menus and a menu bar.
//
//   auto file = std::make_shared<Menu>();
//   file->add("&New File", [&] { ... }, Shortcut::parse("Ctrl+N"));
//   file->add_check("Word &Wrap", false, [&](bool on) { ... }, Shortcut::parse("Alt+Z"));
//   Menu& recent = file->add_submenu("Open &Recent");
//   file->add_separator();
//
//   widget.set_context_menu(file);                 // right-click / Menu key / Shift+F10
//   popup_menu(host, file, window_point);          // or open one yourself
//   bar.add_menu("&File", file);                   // MenuBar (Alt+F opens it, shortcuts work)
//
// "&" in a label marks the mnemonic letter ("&&" is a literal ampersand). The menu model is
// shared: check/radio state lives in the model, so a menu reopens with its current state.
#pragma once
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "uitk/core/event.h"
#include "uitk/render/canvas.h"
#include "uitk/widgets/widget.h"

namespace uitk {

class WidgetHost;

// A key combination: parses and prints "Ctrl+Shift+P", "F5", "Alt+Up", "Ctrl+,".
struct Shortcut {
    uint32_t keysym = 0;
    uint32_t mods = MOD_NONE;

    Shortcut() = default;
    Shortcut(uint32_t key, uint32_t modifiers = MOD_NONE) : keysym(key), mods(modifiers) {}
    static Shortcut parse(std::string_view text);   // invalid text: empty shortcut

    explicit operator bool() const { return keysym != 0; }
    bool matches(const KeyEvent& e) const;   // a key press with exactly these modifiers
    std::string text() const;
    friend bool operator==(const Shortcut&, const Shortcut&) = default;
};

class Menu;

struct MenuItem {
    enum class Kind { Action, Check, Radio, Separator, Submenu };
    Kind kind = Kind::Action;
    std::string label;                      // with "&" mnemonic marker
    Shortcut shortcut;
    Image icon;
    bool enabled = true;
    bool checked = false;                   // Check / Radio
    int group = 0;                          // Radio: exclusive within the menu and group
    std::function<void()> action;           // Action / Radio (on selection)
    std::function<void(bool)> toggled;      // Check
    std::shared_ptr<Menu> submenu;

    MenuItem& set_enabled(bool e) { enabled = e; return *this; }
    MenuItem& set_icon(Image i) { icon = std::move(i); return *this; }
    MenuItem& set_shortcut(Shortcut s) { shortcut = s; return *this; }
    MenuItem& set_checked(bool c) { checked = c; return *this; }

    std::string display_label() const;      // without the "&" markers
    char32_t mnemonic() const;              // lower-case mnemonic letter (0 = none)
    size_t mnemonic_offset() const;         // byte offset of the mnemonic in display_label()
    bool interactive() const { return kind != Kind::Separator && enabled; }
};

class Menu {
public:
    MenuItem& add(std::string label, std::function<void()> action = {}, Shortcut shortcut = {});
    MenuItem& add_check(std::string label, bool checked, std::function<void(bool)> toggled = {}, Shortcut shortcut = {});
    MenuItem& add_radio(int group, std::string label, bool checked, std::function<void()> selected = {}, Shortcut shortcut = {});
    void add_separator();
    Menu& add_submenu(std::string label, std::shared_ptr<Menu> submenu = nullptr);

    std::deque<MenuItem>& items() { return items_; }   // deque: references stay valid on add
    const std::deque<MenuItem>& items() const { return items_; }
    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }
    MenuItem* find(std::string_view display_label);   // searches submenus too

    // Runs an item: toggles checks, selects radios, then calls its callback.
    void activate(size_t index);
    // Finds an enabled item with this shortcut (submenus included) and activates it.
    bool trigger_shortcut(const KeyEvent& e);

private:
    std::deque<MenuItem> items_;
};

// The popup that shows a Menu (opened by popup_menu / MenuBar; submenus open beside it).
class MenuPopup : public Widget {
public:
    MenuPopup(std::shared_ptr<Menu> menu, MenuPopup* parent = nullptr, bool keyboard = false);
    ~MenuPopup() override;

    const std::shared_ptr<Menu>& menu() const { return menu_; }
    int highlighted() const { return highlighted_; }
    void set_highlighted(int index);
    MenuPopup* parent_menu() const { return parent_; }
    MenuPopup* open_submenu() const { return child_; }
    void open_submenu(int index, bool keyboard);
    void activate(int index);   // closes the whole menu chain, then runs the item
    Rect item_rect(int index) const;   // local coordinates

    // Left/Right at the top level of a MenuBar menu: switch to the neighbouring menu.
    std::function<void(int direction)> navigate;

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;
    bool on_text(const TextEvent& e) override;

    static constexpr float kPad = 4;

private:
    float item_height(const MenuItem& it, const Theme& t) const;
    int item_at(Point p) const;
    int step(int from, int dir) const;   // next interactive item (wraps)
    void close_submenu();
    void hover_item(int index);
    void cancel_timer();
    MenuPopup* root();

    std::shared_ptr<Menu> menu_;
    MenuPopup* parent_;
    MenuPopup* child_ = nullptr;
    int child_index_ = -1;
    int highlighted_ = -1;
    bool keyboard_;   // opened from the keyboard: show mnemonics, highlight the first item
    bool pressed_ = false;
    uint64_t timer_ = 0;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

// Opens `menu` as a context menu at a window position (top-left at the point).
MenuPopup& popup_menu(WidgetHost& host, std::shared_ptr<Menu> menu, Point window_pos, bool keyboard = false);
// Opens `menu` below `anchor` (window coordinates), e.g. under a button.
MenuPopup& popup_menu(WidgetHost& host, std::shared_ptr<Menu> menu, const Rect& anchor, bool keyboard,
                      std::function<void()> on_close = {});

// Horizontal menu bar ("File Edit View ..."). Click or Alt+mnemonic opens a menu; while one is
// open, hovering another title switches to it and Left/Right move between menus. Shortcuts of
// all its items work application-wide (when no focused widget used the key). F10 opens the first.
class MenuBar : public Widget {
public:
    MenuBar();
    ~MenuBar() override;

    Menu& add_menu(std::string title, std::shared_ptr<Menu> menu = nullptr);
    size_t count() const { return entries_.size(); }
    Menu& menu(size_t i) { return *entries_[i].menu; }
    void open(int index, bool keyboard);
    void close();
    int open_index() const { return open_; }
    bool trigger_shortcut(const KeyEvent& e);

    bool accepts_pointer() const override { return true; }
    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;

protected:
    void on_state_changed(uint32_t) override;

private:
    struct Entry {
        MenuItem title;   // reuses label/mnemonic parsing
        std::shared_ptr<Menu> menu;
        Rect rect;
    };
    int entry_at(Point p) const;
    bool handle_key(const KeyEvent& e);
    void register_keys();

    std::vector<Entry> entries_;
    int open_ = -1, hover_ = -1;
    bool keyboard_open_ = false;
    MenuPopup* popup_ = nullptr;
    WidgetHost* key_host_ = nullptr;
    uint64_t key_handler_ = 0;
};

} // namespace uitk
