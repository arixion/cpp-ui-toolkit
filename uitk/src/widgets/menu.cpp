#include "uitk/widgets/menu.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "uitk/core/application.h"
#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/widget_host.h"

namespace uitk {

namespace {
constexpr uint32_t kSpace = 0x20, kKpEnter = 0xff8d, kF10 = 0xffc7, kInsert = 0xff63;
constexpr float kLeftColumn = 28, kRightPad = 12, kShortcutGap = 36, kArrowColumn = 20;
constexpr int kSubmenuDelayMs = 200;

std::string lower(std::string_view s) {
    std::string o(s);
    for (auto& c : o) c = (char)std::tolower((unsigned char)c);
    return o;
}

struct KeyName { const char* name; uint32_t sym; };
constexpr KeyName kKeyNames[] = {
    {"Up", key::Up}, {"Down", key::Down}, {"Left", key::Left}, {"Right", key::Right},
    {"Home", key::Home}, {"End", key::End}, {"PageUp", key::PageUp}, {"PageDown", key::PageDown},
    {"Delete", key::Delete}, {"Backspace", key::BackSpace}, {"Enter", key::Return},
    {"Escape", key::Escape}, {"Tab", key::Tab}, {"Space", kSpace}, {"Insert", kInsert},
};
struct Alias { const char* name; uint32_t sym; };
constexpr Alias kAliases[] = {
    {"pgup", key::PageUp}, {"pgdn", key::PageDown}, {"del", key::Delete}, {"return", key::Return},
    {"esc", key::Escape}, {"ins", kInsert},
};

Size text_size(const std::string& s, const TextStyle& st) { return TextLayout(s, st).size(); }
} // namespace

// ============================================================================ Shortcut

Shortcut Shortcut::parse(std::string_view text) {
    Shortcut sc;
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '+' && !cur.empty()) { parts.push_back(cur); cur.clear(); }
        else cur += c;   // a '+' at the start of a part is the key itself ("Ctrl++")
    }
    if (!cur.empty()) parts.push_back(cur);
    if (parts.empty()) return {};
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        std::string m = lower(parts[i]);
        if (m == "ctrl" || m == "control") sc.mods |= mod::Ctrl;
        else if (m == "shift") sc.mods |= mod::Shift;
        else if (m == "alt") sc.mods |= mod::Alt;
        else if (m == "super" || m == "meta" || m == "win") sc.mods |= mod::Super;
        else return {};
    }
    std::string k = parts.back(), lk = lower(k);
    if (k.size() == 1) { sc.keysym = (uint32_t)(unsigned char)std::tolower((unsigned char)k[0]); return sc; }
    if (lk.size() >= 2 && lk[0] == 'f' && std::all_of(lk.begin() + 1, lk.end(), ::isdigit)) {
        int n = std::stoi(lk.substr(1));
        if (n >= 1 && n <= 24) { sc.keysym = key::F1 + (uint32_t)(n - 1); return sc; }
    }
    for (auto& kn : kKeyNames) if (lower(kn.name) == lk) { sc.keysym = kn.sym; return sc; }
    for (auto& a : kAliases) if (a.name == lk) { sc.keysym = a.sym; return sc; }
    return {};
}

bool Shortcut::matches(const KeyEvent& e) const {
    if (!keysym || !e.pressed) return false;
    uint32_t k = e.keysym;
    if (k >= 'A' && k <= 'Z') k = k - 'A' + 'a';
    if (k == kKpEnter) k = key::Return;
    return k == keysym && (e.mods & (mod::Ctrl | mod::Shift | mod::Alt | mod::Super)) == mods;
}

std::string Shortcut::text() const {
    if (!keysym) return {};
    std::string s;
    if (mods & mod::Ctrl) s += "Ctrl+";
    if (mods & mod::Alt) s += "Alt+";
    if (mods & mod::Shift) s += "Shift+";
    if (mods & mod::Super) s += "Super+";
    if (keysym >= key::F1 && keysym < key::F1 + 24) return s + "F" + std::to_string(keysym - key::F1 + 1);
    for (auto& kn : kKeyNames) if (kn.sym == keysym) return s + kn.name;
    if (keysym < 0x80) s += (char)std::toupper((int)keysym);
    return s;
}

// ============================================================================ MenuItem / Menu

std::string MenuItem::display_label() const {
    std::string out;
    for (size_t i = 0; i < label.size(); ++i) {
        if (label[i] == '&' && i + 1 < label.size()) { out += label[++i] == '&' ? "&" : std::string(1, label[i]); continue; }
        out += label[i];
    }
    return out;
}

char32_t MenuItem::mnemonic() const {
    for (size_t i = 0; i + 1 < label.size(); ++i) {
        if (label[i] != '&') continue;
        if (label[i + 1] == '&') { ++i; continue; }
        unsigned char c = (unsigned char)label[i + 1];
        return c < 0x80 ? (char32_t)std::tolower(c) : 0;
    }
    return 0;
}

size_t MenuItem::mnemonic_offset() const {
    size_t out = 0;
    for (size_t i = 0; i + 1 < label.size(); ++i) {
        if (label[i] == '&') {
            if (label[i + 1] != '&') return out;
            ++i;
        }
        ++out;
    }
    return std::string::npos;
}

MenuItem& Menu::add(std::string label, std::function<void()> action, Shortcut shortcut) {
    MenuItem& it = items_.emplace_back();
    it.label = std::move(label);
    it.action = std::move(action);
    it.shortcut = shortcut;
    return it;
}

MenuItem& Menu::add_check(std::string label, bool checked, std::function<void(bool)> toggled, Shortcut shortcut) {
    MenuItem& it = add(std::move(label), {}, shortcut);
    it.kind = MenuItem::Kind::Check;
    it.checked = checked;
    it.toggled = std::move(toggled);
    return it;
}

MenuItem& Menu::add_radio(int group, std::string label, bool checked, std::function<void()> selected, Shortcut shortcut) {
    MenuItem& it = add(std::move(label), std::move(selected), shortcut);
    it.kind = MenuItem::Kind::Radio;
    it.group = group;
    it.checked = checked;
    return it;
}

void Menu::add_separator() { items_.emplace_back().kind = MenuItem::Kind::Separator; }

Menu& Menu::add_submenu(std::string label, std::shared_ptr<Menu> submenu) {
    MenuItem& it = add(std::move(label));
    it.kind = MenuItem::Kind::Submenu;
    it.submenu = submenu ? std::move(submenu) : std::make_shared<Menu>();
    return *it.submenu;
}

MenuItem* Menu::find(std::string_view display_label) {
    for (auto& it : items_) {
        if (it.kind != MenuItem::Kind::Separator && it.display_label() == display_label) return &it;
        if (it.submenu) if (MenuItem* f = it.submenu->find(display_label)) return f;
    }
    return nullptr;
}

void Menu::activate(size_t index) {
    if (index >= items_.size() || !items_[index].interactive()) return;
    MenuItem& it = items_[index];
    switch (it.kind) {
        case MenuItem::Kind::Check: {
            it.checked = !it.checked;
            auto fn = it.toggled;
            if (fn) fn(it.checked);
            return;
        }
        case MenuItem::Kind::Radio:
            for (auto& o : items_) if (o.kind == MenuItem::Kind::Radio && o.group == it.group) o.checked = false;
            it.checked = true;
            [[fallthrough]];
        case MenuItem::Kind::Action: {
            auto fn = it.action;   // the callback may change the menu
            if (fn) fn();
            return;
        }
        default: return;
    }
}

bool Menu::trigger_shortcut(const KeyEvent& e) {
    for (size_t i = 0; i < items_.size(); ++i) {
        MenuItem& it = items_[i];
        if (!it.enabled) continue;
        if (it.kind == MenuItem::Kind::Submenu) {
            if (it.submenu && it.submenu->trigger_shortcut(e)) return true;
        } else if (it.shortcut.matches(e)) {
            activate(i);
            return true;
        }
    }
    return false;
}

// ============================================================================ MenuPopup

MenuPopup::MenuPopup(std::shared_ptr<Menu> menu, MenuPopup* parent, bool keyboard)
    : menu_(std::move(menu)), parent_(parent), keyboard_(keyboard) {
    if (keyboard_) highlighted_ = step(-1, 1);
}

MenuPopup::~MenuPopup() {
    *alive_ = false;
    cancel_timer();
}

void MenuPopup::cancel_timer() {
    if (timer_) if (Application* app = Application::current()) app->cancel_timer(timer_);
    timer_ = 0;
}

MenuPopup* MenuPopup::root() {
    MenuPopup* r = this;
    while (r->parent_) r = r->parent_;
    return r;
}

float MenuPopup::item_height(const MenuItem& it, const Theme& t) const {
    return it.kind == MenuItem::Kind::Separator ? 9.f : t.control_height - 2;
}

Size MenuPopup::measure(float, const Theme& t) {
    TextStyle s = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
    float label_w = 0, sc_w = 0, h = 2 * kPad;
    bool arrows = false;
    for (auto& it : menu_->items()) {
        h += item_height(it, t);
        if (it.kind == MenuItem::Kind::Separator) continue;
        label_w = std::max(label_w, text_size(it.display_label(), s).width);
        if (it.shortcut) sc_w = std::max(sc_w, text_size(it.shortcut.text(), s).width);
        arrows |= it.kind == MenuItem::Kind::Submenu;
    }
    float w = 2 * kPad + kLeftColumn + std::ceil(label_w) + kRightPad;
    if (sc_w > 0) w += kShortcutGap + std::ceil(sc_w);
    if (arrows) w += kArrowColumn;
    return {std::max(w, 180.f), h};
}

Rect MenuPopup::item_rect(int index) const {
    const Theme& t = theme();
    float y = kPad;
    auto& items = menu_->items();
    for (int i = 0; i < (int)items.size(); ++i) {
        float h = item_height(items[(size_t)i], t);
        if (i == index) return {kPad, y, size().width - 2 * kPad, h};
        y += h;
    }
    return {};
}

int MenuPopup::item_at(Point p) const {
    if (p.x < 0 || p.x >= size().width) return -1;
    const Theme& t = theme();
    float y = kPad;
    auto& items = menu_->items();
    for (int i = 0; i < (int)items.size(); ++i) {
        float h = item_height(items[(size_t)i], t);
        if (p.y >= y && p.y < y + h) return items[(size_t)i].kind == MenuItem::Kind::Separator ? -1 : i;
        y += h;
    }
    return -1;
}

int MenuPopup::step(int from, int dir) const {
    int n = (int)menu_->size();
    if (n == 0) return -1;
    if (from < 0) from = dir > 0 ? -1 : n;
    for (int k = 1; k <= n; ++k) {
        int j = ((from + dir * k) % n + n) % n;
        if (menu_->items()[(size_t)j].interactive()) return j;
    }
    return -1;
}

void MenuPopup::set_highlighted(int index) {
    if (index == highlighted_) return;
    highlighted_ = index;
    invalidate();
}

void MenuPopup::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    Rect r = local_rect();
    ctx.canvas.draw_rrect(r, t.popup_radius, Paint::fill(t.popup_bg));
    ctx.canvas.draw_rrect(r.inset(0.5f, 0.5f), t.popup_radius - 0.5f, Paint::stroke(t.popup_border, 1));
    TextStyle base = Label::style_for(t, TextRole::Body, TextAlign::Left, true);
    float y = kPad;
    auto& items = menu_->items();
    for (int i = 0; i < (int)items.size(); ++i) {
        const MenuItem& it = items[(size_t)i];
        float h = item_height(it, t);
        Rect row{kPad, y, r.width - 2 * kPad, h};
        y += h;
        if (it.kind == MenuItem::Kind::Separator) {
            ctx.canvas.draw_rect({row.x + 6, std::floor(row.y + h / 2), row.width - 12, 1}, Paint::fill(t.popup_border));
            continue;
        }
        bool hi = (i == highlighted_ || i == child_index_) && it.enabled;
        if (hi) ctx.canvas.draw_rrect(row, t.radius, Paint::fill(t.selection_bg));
        Color fg = hi ? t.selection_fg : t.foreground;
        Color dim = hi ? t.selection_fg_dim : t.foreground_dim;
        if (!it.enabled) fg = dim = t.foreground_dim.faded(t.disabled_opacity + 0.2f);
        Rect lead{row.x + std::round((kLeftColumn - 16) / 2), row.y + std::round((h - 16) / 2), 16, 16};
        if (it.kind == MenuItem::Kind::Check && it.checked) ctx.canvas.draw_image(glyph(Glyph::Check), lead, fg);
        else if (it.kind == MenuItem::Kind::Radio && it.checked) ctx.canvas.draw_circle(lead.center(), 3.f, Paint::fill(fg));
        else if (it.icon.valid()) ctx.canvas.draw_image(it.icon, lead, fg);

        TextStyle s = base;
        s.color = fg;
        TextLayout tl(it.display_label(), s);
        float tx = row.x + kLeftColumn, ty = row.y + std::round((h - tl.size().height) / 2);
        ctx.canvas.draw_text(tl, {tx, ty});
        size_t mo = it.mnemonic_offset();
        if (keyboard_ && mo != std::string::npos) {
            auto rects = tl.range_rects(mo, mo + 1);
            if (!rects.empty())
                ctx.canvas.draw_rect({tx + rects[0].x, ty + rects[0].bottom() - 1, rects[0].width, 1}, Paint::fill(fg));
        }
        float right = row.right() - kRightPad;
        if (it.kind == MenuItem::Kind::Submenu) {
            ctx.canvas.draw_image(glyph(Glyph::ChevronRight), {row.right() - 4 - 16, row.y + std::round((h - 16) / 2), 16, 16}, fg);
        } else if (it.shortcut) {
            TextStyle ss = base;
            ss.color = dim;
            TextLayout sl(it.shortcut.text(), ss);
            ctx.canvas.draw_text(sl, {right - sl.size().width, row.y + std::round((h - sl.size().height) / 2)});
        }
    }
}

void MenuPopup::close_submenu() {
    if (child_ && host()) host()->close_popup(child_);
    child_ = nullptr;
    child_index_ = -1;
}

void MenuPopup::open_submenu(int index, bool keyboard) {
    if (index < 0 || index >= (int)menu_->size()) return;
    const MenuItem& it = menu_->items()[(size_t)index];
    if (it.kind != MenuItem::Kind::Submenu || !it.enabled || !it.submenu) return;
    if (child_ && child_index_ == index) {
        if (keyboard && host()) {   // already open (by hover): move into it
            child_->set_highlighted(child_->step(-1, 1));
            host()->set_focus(child_, true);
        }
        return;
    }
    close_submenu();
    WidgetHost* h = host();
    if (!h) return;
    Rect row = item_rect(index);
    Point o = to_window({0, row.y - kPad});
    WidgetHost::PopupOptions po;
    po.submenu = true;
    po.placement = WidgetHost::PopupOptions::Placement::Right;
    po.gap = 2;
    std::weak_ptr<bool> alive = alive_;
    po.on_close = [this, alive, index] {
        if (alive.expired() || child_index_ != index) return;
        child_ = nullptr;
        child_index_ = -1;
        invalidate();
    };
    highlighted_ = index;
    child_index_ = index;
    child_ = &h->emplace_popup<MenuPopup>(Rect{o.x, o.y, size().width, row.height}, std::move(po), it.submenu, this, keyboard);
    invalidate();
}

void MenuPopup::hover_item(int index) {
    if (index < 0) return;   // padding / separators: keep the highlight
    set_highlighted(index);
    cancel_timer();
    if (child_ && child_index_ == index) return;
    bool sub = menu_->items()[(size_t)index].kind == MenuItem::Kind::Submenu;
    if (!child_ && !sub) return;
    // Open (or switch) submenus after a short delay, so moving diagonally into an open submenu
    // across other items does not close it.
    auto apply = [this, index] {
        if (highlighted_ != index) return;
        if (child_ && child_index_ != index) close_submenu();
        open_submenu(index, false);
    };
    Application* app = Application::current();
    if (!app) { apply(); return; }
    std::weak_ptr<bool> alive = alive_;
    timer_ = app->add_timer(std::chrono::milliseconds(kSubmenuDelayMs), [this, alive, apply] {
        if (alive.expired()) return;
        timer_ = 0;
        apply();
    });
}

void MenuPopup::activate(int index) {
    if (index < 0 || index >= (int)menu_->size()) return;
    const MenuItem& it = menu_->items()[(size_t)index];
    if (!it.interactive()) return;
    if (it.kind == MenuItem::Kind::Submenu) { open_submenu(index, true); return; }
    std::shared_ptr<Menu> m = menu_;
    WidgetHost* h = host();
    if (h) h->close_popup(root());   // destroys this popup and its parents
    m->activate((size_t)index);      // after closing: the action may open a dialog
}

bool MenuPopup::on_pointer(const PointerEvent& e) {
    int i = item_at(e.pos);
    switch (e.type) {
        case PointerEvent::Type::Enter:
        case PointerEvent::Type::Move: hover_item(i); return true;
        case PointerEvent::Type::Down: pressed_ = true; return true;
        case PointerEvent::Type::Up:
            // Only a release that started in this menu activates (not the release of the
            // right-click that opened a context menu under the pointer).
            if (pressed_ && i >= 0) {
                pressed_ = false;
                if (menu_->items()[(size_t)i].kind == MenuItem::Kind::Submenu) open_submenu(i, false);
                else activate(i);   // may destroy this
                return true;
            }
            pressed_ = false;
            return true;
        default: return true;
    }
}

bool MenuPopup::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    switch (e.keysym) {
        case key::Up:   set_highlighted(step(highlighted_, -1)); return true;
        case key::Down: set_highlighted(step(highlighted_, 1)); return true;
        case key::Home: set_highlighted(step(-1, 1)); return true;
        case key::End:  set_highlighted(step(-1, -1)); return true;
        case key::Right:
            if (highlighted_ >= 0 && menu_->items()[(size_t)highlighted_].kind == MenuItem::Kind::Submenu) {
                open_submenu(highlighted_, true);
            } else if (!parent_ && navigate) {
                auto nav = navigate;   // this popup is closed by the switch
                nav(1);
            }
            return true;
        case key::Left:
            if (parent_) { if (host()) host()->close_popup(this); }
            else if (navigate) { auto nav = navigate; nav(-1); }
            return true;
        case key::Return: case kKpEnter: case kSpace:
            if (highlighted_ >= 0) activate(highlighted_);   // may destroy this
            return true;
    }
    return e.keysym != key::Tab;   // keep keys inside the menu
}

bool MenuPopup::on_text(const TextEvent& e) {
    if (e.utf8.size() != 1) return true;
    char32_t c = (char32_t)std::tolower((unsigned char)e.utf8[0]);
    auto& items = menu_->items();
    int match = -1, count = 0;
    for (int i = 0; i < (int)items.size(); ++i)
        if (items[(size_t)i].interactive() && items[(size_t)i].mnemonic() == c) { if (match < 0) match = i; ++count; }
    if (count == 1) { activate(match); return true; }   // may destroy this
    // No (unique) mnemonic: cycle through items starting with the letter.
    int n = (int)items.size();
    for (int k = 1; k <= n; ++k) {
        int j = ((highlighted_ < 0 ? -1 : highlighted_) + k + n) % n;
        const MenuItem& it = items[(size_t)j];
        std::string l = it.display_label();
        if (it.interactive() && !l.empty() && (char32_t)std::tolower((unsigned char)l[0]) == c) { set_highlighted(j); break; }
    }
    return true;
}

// ---------------------------------------------------------------------------- popup_menu

MenuPopup& popup_menu(WidgetHost& host, std::shared_ptr<Menu> menu, Point p, bool keyboard) {
    WidgetHost::PopupOptions po;
    po.gap = 0;
    return host.emplace_popup<MenuPopup>(Rect{p.x, p.y, 0, 0}, std::move(po), std::move(menu), nullptr, keyboard);
}

MenuPopup& popup_menu(WidgetHost& host, std::shared_ptr<Menu> menu, const Rect& anchor, bool keyboard,
                      std::function<void()> on_close) {
    WidgetHost::PopupOptions po;
    po.gap = 2;
    po.on_close = std::move(on_close);
    return host.emplace_popup<MenuPopup>(anchor, std::move(po), std::move(menu), nullptr, keyboard);
}

// ============================================================================ MenuBar

namespace {
constexpr float kTitlePadX = 8;
}

MenuBar::MenuBar() { set_align(Align::Start, Align::Center); }

MenuBar::~MenuBar() {
    WidgetHost* h = host();
    if (h && h == key_host_) h->remove_key_handler(key_handler_);
    if (h && popup_) { MenuPopup* p = popup_; popup_ = nullptr; open_ = -1; h->close_popup(p); }
}

Menu& MenuBar::add_menu(std::string title, std::shared_ptr<Menu> menu) {
    Entry e;
    e.title.label = std::move(title);
    e.menu = menu ? std::move(menu) : std::make_shared<Menu>();
    entries_.push_back(std::move(e));
    invalidate_layout();
    return *entries_.back().menu;
}

Size MenuBar::measure(float, const Theme& t) {
    TextStyle s = Label::style_for(t, TextRole::Chrome, TextAlign::Left, true);
    float w = 0;
    for (auto& e : entries_) w += std::ceil(text_size(e.title.display_label(), s).width) + 2 * kTitlePadX;
    return {w, t.control_height - 4};
}

void MenuBar::layout(const Theme& t) {
    register_keys();
    TextStyle s = Label::style_for(t, TextRole::Chrome, TextAlign::Left, true);
    float x = 0;
    for (auto& e : entries_) {
        float w = std::ceil(text_size(e.title.display_label(), s).width) + 2 * kTitlePadX;
        e.rect = {x, 0, w, size().height};
        x += w;
    }
}

void MenuBar::register_keys() {
    WidgetHost* h = host();
    if (h == key_host_) return;
    if (key_host_ && key_handler_) key_host_->remove_key_handler(key_handler_);
    key_host_ = h;
    key_handler_ = h ? h->add_key_handler([this](const KeyEvent& e) { return handle_key(e); }) : 0;
}

void MenuBar::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    TextStyle s = Label::style_for(t, TextRole::Chrome, TextAlign::Left, true);
    s.color = ctx.backdrop(t.chrome_fg);
    for (int i = 0; i < (int)entries_.size(); ++i) {
        const Entry& e = entries_[(size_t)i];
        if (i == open_ || (i == hover_ && enabled()))
            ctx.canvas.draw_rrect(e.rect, t.radius,
                                  Paint::fill(t.button_flat.overlay.with_opacity(i == open_ ? t.hover_opacity + t.press_opacity : t.hover_opacity)));
        TextLayout tl(e.title.display_label(), s);
        Point o{e.rect.x + kTitlePadX, std::round((e.rect.height - tl.size().height) / 2)};
        ctx.canvas.draw_text(tl, o);
        size_t mo = e.title.mnemonic_offset();
        if (keyboard_open_ && i == open_ && mo != std::string::npos) {
            auto rects = tl.range_rects(mo, mo + 1);
            if (!rects.empty()) ctx.canvas.draw_rect({o.x + rects[0].x, o.y + rects[0].bottom() - 1, rects[0].width, 1}, Paint::fill(s.color));
        }
    }
}

int MenuBar::entry_at(Point p) const {
    for (int i = 0; i < (int)entries_.size(); ++i)
        if (entries_[(size_t)i].rect.contains(p)) return i;
    return -1;
}

void MenuBar::on_state_changed(uint32_t) {
    if (!(state() & STATE_HOVERED) && hover_ != -1) { hover_ = -1; invalidate(); }
}

bool MenuBar::on_pointer(const PointerEvent& e) {
    int i = entry_at(e.pos);
    switch (e.type) {
        case PointerEvent::Type::Enter:
        case PointerEvent::Type::Move:
            if (i != hover_) { hover_ = i; invalidate(); }
            if (open_ >= 0 && i >= 0 && i != open_) open(i, false);   // slide between menus
            return true;
        case PointerEvent::Type::Down:
            if (e.button == PointerButton::Left && i >= 0) { if (open_ == i) close(); else open(i, false); }
            return true;
        default: return true;
    }
}

void MenuBar::open(int index, bool keyboard) {
    WidgetHost* h = host();
    if (!h || index < 0 || index >= (int)entries_.size()) return;
    if (popup_) close();
    Entry& e = entries_[(size_t)index];
    Point o = to_window(e.rect.origin());
    open_ = index;
    keyboard_open_ = keyboard;
    MenuPopup* p = &popup_menu(*h, e.menu, Rect{o.x, o.y, e.rect.width, e.rect.height}, keyboard, [this, index] {
        if (open_ != index) return;
        open_ = -1;
        popup_ = nullptr;
        invalidate();
    });
    popup_ = p;
    p->navigate = [this](int dir) {
        int n = (int)entries_.size();
        open((open_ + dir + n) % n, true);
    };
    invalidate();
}

void MenuBar::close() {
    WidgetHost* h = host();
    MenuPopup* p = popup_;
    popup_ = nullptr;
    open_ = -1;
    if (h && p) h->close_popup(p);
    invalidate();
}

bool MenuBar::trigger_shortcut(const KeyEvent& e) {
    if (!enabled()) return false;
    for (auto& en : entries_) if (en.menu->trigger_shortcut(e)) return true;
    return false;
}

bool MenuBar::handle_key(const KeyEvent& e) {
    if (!e.pressed || !visible() || !enabled()) return false;
    uint32_t mods = e.mods & (mod::Ctrl | mod::Shift | mod::Alt | mod::Super);
    if (mods == mod::Alt && e.keysym < 0x80) {
        char32_t c = (char32_t)std::tolower((int)e.keysym);
        for (int i = 0; i < (int)entries_.size(); ++i)
            if (entries_[(size_t)i].title.mnemonic() == c) { open(i, true); return true; }
    }
    if (mods == 0 && e.keysym == kF10 && !entries_.empty()) { open(0, true); return true; }
    return trigger_shortcut(e);
}

} // namespace uitk
