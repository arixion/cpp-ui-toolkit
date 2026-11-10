#include "uitk/widgets/search_field.h"

#include <algorithm>

#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/icon.h"
#include "uitk/widgets/icons.h"
#include "uitk/widgets/text_field.h"
#include "uitk/widgets/toggles.h"

namespace uitk {

namespace {
constexpr size_t kHistoryLimit = 30;
constexpr float kInline = 20;   // inline button size
}

SearchField::SearchField(std::string placeholder) : Box(Orientation::Horizontal, 2.f) {
    set_padding(Insets{6, 0, 3, 0});
    set_align(Align::Fill, Align::Center);
    icon_ = &add<Icon>(icons::symbolic("edit-find"), 14);
    icon_->set_token(ColorToken::ForegroundDim);
    icon_->set_align(Align::Start, Align::Center);
    field_ = &add<TextField>("", std::move(placeholder));
    field_->set_frameless(true);
    field_->set_expand(true, false);
    field_->set_min_size({60, 0});
    field_->changed.connect([this](const std::string& t) { text_edited(t); });
    field_->activated.connect([this] {
        flush();
        remember(field_->text());
        activated.emit(field_->text());
    });
    clear_ = &add<Button>(glyph(Glyph::Close), ButtonStyle::Flat);
    clear_->set_fixed_size({kInline, kInline});
    clear_->set_icon_size(14);
    clear_->set_focusable(false);
    clear_->set_tooltip("Clear");
    clear_->set_align(Align::Start, Align::Center);
    clear_->set_visible(false);
    clear_->clicked.connect([this] { clear(); field_->grab_focus(); });
    options_ = &add<Box>(Orientation::Horizontal, 1.f);
    options_->set_align(Align::Start, Align::Center);
}

SearchField::~SearchField() {
    if (timer_) if (Application* app = Application::current()) app->cancel_timer(timer_);
}

const std::string& SearchField::text() const { return field_->text(); }

void SearchField::set_text(std::string t) {
    field_->set_text(std::move(t));   // emits through text_edited (unless unchanged)
    clear_->set_visible(!field_->text().empty());
    if (timer_) flush();               // no debounce for programmatic changes
}

ToggleButton& SearchField::add_option(Image icon, std::string tooltip) {
    ToggleButton& b = options_->add<ToggleButton>(std::move(icon), ButtonStyle::Flat);
    b.set_fixed_size({kInline, kInline});
    b.set_icon_size(14);
    b.set_tooltip(std::move(tooltip));
    b.set_focusable(false);
    return b;
}

void SearchField::text_edited(const std::string& t) {
    clear_->set_visible(!t.empty());
    history_pos_ = -1;
    Application* app = Application::current();
    if (debounce_ms_ <= 0 || !app) { flush(); return; }
    if (timer_) app->cancel_timer(timer_);
    timer_ = app->add_timer(std::chrono::milliseconds(debounce_ms_), [this] { timer_ = 0; flush(); });
}

void SearchField::flush() {
    if (timer_) { if (Application* app = Application::current()) app->cancel_timer(timer_); timer_ = 0; }
    search_changed.emit(field_->text());
}

void SearchField::remember(const std::string& t) {
    if (!history_enabled_ || t.empty()) return;
    std::erase(history_, t);
    history_.push_back(t);
    if (history_.size() > kHistoryLimit) history_.erase(history_.begin());
    history_pos_ = -1;
}

void SearchField::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    Rect r = local_rect();
    bool focused = field_->has_focus();
    ctx.canvas.draw_rrect(r, t.radius, Paint::fill(t.control_bg.faded(enabled() ? 1.f : t.disabled_opacity)));
    Color border = focused ? t.focus_ring : t.control_border;
    ctx.canvas.draw_rrect(r.inset(0.5f, 0.5f), t.radius - 0.5f, Paint::stroke(border, 1));
}

bool SearchField::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    if (e.keysym == key::Escape && !field_->text().empty()) { clear(); return true; }
    if (history_enabled_ && !history_.empty() && (e.keysym == key::Up || e.keysym == key::Down) && !e.mods) {
        long n = (long)history_.size();
        if (e.keysym == key::Up) history_pos_ = history_pos_ < 0 ? n - 1 : std::max(0L, history_pos_ - 1);
        else history_pos_ = history_pos_ < 0 ? -1 : (history_pos_ + 1 >= n ? -1 : history_pos_ + 1);
        long keep = history_pos_;
        set_text(keep >= 0 ? history_[(size_t)keep] : std::string());
        history_pos_ = keep;   // set_text -> changed resets it
        return true;
    }
    return Box::on_key(e);
}

} // namespace uitk
