#include "uitk/widgets/list_view.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace uitk {

namespace {
constexpr uint32_t kSpace = 0x20, kKpEnter = 0xff8d;
std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
}

ListView::ListView() { set_align(Align::Fill, Align::Start); }

ListView::~ListView() {
    if (model_) model_->changed.disconnect(model_conn_);
}

void ListView::set_model(ListModel* m) {
    if (model_) model_->changed.disconnect(model_conn_);
    model_ = m;
    if (model_) model_conn_ = model_->changed.connect([this] { model_changed(); });
    model_changed();
}

void ListView::model_changed() {
    size_t n = count();
    selected_.assign(n, false);   // rows changed meaning: the owner re-selects if needed
    cursor_ = anchor_ = 0;
    hover_ = -1;
    invalidate_layout();
}

float ListView::row_height() const { return row_height_ > 0 ? row_height_ : theme().list_row_height; }

void ListView::set_selection_mode(SelectionMode m) {
    mode_ = m;
    clear_selection();
}

std::vector<size_t> ListView::selection() const {
    std::vector<size_t> out;
    for (size_t i = 0; i < selected_.size(); ++i) if (selected_[i]) out.push_back(i);
    return out;
}

void ListView::select_only(size_t i) {
    if (mode_ == SelectionMode::None || i >= count() || !model_->row_enabled(i)) return;
    std::fill(selected_.begin(), selected_.end(), false);
    selected_[i] = true;
    cursor_ = anchor_ = i;
    invalidate();
    selection_changed.emit();
}

void ListView::clear_selection() {
    if (std::none_of(selected_.begin(), selected_.end(), [](bool b) { return b; })) return;
    std::fill(selected_.begin(), selected_.end(), false);
    invalidate();
    selection_changed.emit();
}

void ListView::select_all() {
    if (mode_ != SelectionMode::Multiple) return;
    for (size_t i = 0; i < selected_.size(); ++i) selected_[i] = model_->row_enabled(i);
    invalidate();
    selection_changed.emit();
}

void ListView::set_current(size_t i, bool scroll) {
    if (i >= count()) return;
    cursor_ = i;
    float h = row_height();
    if (scroll) scroll_into_view({0, i * h, size().width, h});
    invalidate();
}

void ListView::set_selection(const std::vector<size_t>& rows, size_t current) {
    std::fill(selected_.begin(), selected_.end(), false);
    for (size_t r : rows) if (r < selected_.size() && mode_ != SelectionMode::None) selected_[r] = true;
    if (current < count()) cursor_ = anchor_ = current;
    invalidate();
}

void ListView::select_range(size_t a, size_t b, bool additive) {
    if (!additive) std::fill(selected_.begin(), selected_.end(), false);
    for (size_t i = std::min(a, b); i <= std::max(a, b) && i < selected_.size(); ++i)
        selected_[i] = model_->row_enabled(i);
}

// ---------------------------------------------------------------------------- layout / paint

Size ListView::measure(float available_width, const Theme& t) {
    float h = row_height_ > 0 ? row_height_ : t.list_row_height;
    return {std::max(available_width < 1e8f ? available_width : 0.f, 100.f), std::max<float>(count() * h, 4 * h)};
}

long ListView::row_at(float y) const {
    long i = (long)std::floor(y / row_height());
    return (i >= 0 && i < (long)count()) ? i : -1;
}

void ListView::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    if (count() == 0) {
        if (placeholder_.empty()) return;
        Rect vis = visible_rect();
        TextStyle s = t.text(TextRole::Dim);
        s.align = TextAlign::Center;
        TextLayout tl(placeholder_, s, std::max(0.f, vis.width - 40));
        ctx.canvas.draw_text(tl, {vis.x + 20, vis.y + std::max(24.f, vis.height / 3)});
        return;
    }
    float h = row_height();
    // Only the rows inside the visible part (virtualization).
    Rect vis = visible_rect();
    if (vis.empty()) return;
    long first = std::max(0L, (long)std::floor(vis.y / h));
    long last = std::min((long)count() - 1, (long)std::floor((vis.bottom() - 1) / h));
    bool focused = has_focus();
    for (long i = first; i <= last; ++i) {
        Rect row{0, i * h, size().width, h};
        RowState st{is_selected((size_t)i), i == hover_, (size_t)i == cursor_ && focused, focused};
        Color bg = Color::transparent();
        if (st.selected) bg = t.selection_bg;
        if (st.hovered) bg = Color::over(t.button_flat.overlay.with_opacity(t.hover_opacity * 0.6f), bg);
        if (bg.a) ctx.canvas.draw_rrect(row.inset(2, 0.5f), t.list_radius, Paint::fill(bg));
        if (st.cursor && (state() & STATE_FOCUS_VISIBLE))
            ctx.canvas.draw_rrect(row.inset(2.5f, 1), t.list_radius, Paint::stroke(t.focus_ring, 1));
        model_->paint_row(ctx, (size_t)i, row, st);
    }
}

// ---------------------------------------------------------------------------- input

void ListView::on_state_changed(uint32_t) {
    if (!(state() & STATE_HOVERED) && hover_ != -1) { hover_ = -1; invalidate(); }
}

void ListView::click_select(size_t i, uint32_t mods) {
    if (mode_ == SelectionMode::None || !model_->row_enabled(i)) { cursor_ = i; invalidate(); return; }
    if (mode_ == SelectionMode::Multiple && (mods & MOD_CTRL)) {
        selected_[i] = !selected_[i];
        cursor_ = anchor_ = i;
    } else if (mode_ == SelectionMode::Multiple && (mods & MOD_SHIFT)) {
        select_range(anchor_, i, false);
        cursor_ = i;
    } else {
        std::fill(selected_.begin(), selected_.end(), false);
        selected_[i] = true;
        cursor_ = anchor_ = i;
    }
    invalidate();
    selection_changed.emit();
}

bool ListView::on_pointer(const PointerEvent& e) {
    long i = row_at(e.pos.y);
    switch (e.type) {
        case PointerEvent::Type::Move:
        case PointerEvent::Type::Enter:
            if (i != hover_) { hover_ = i; invalidate(); }
            return true;
        case PointerEvent::Type::Down:
            // Right press: select the row first (context menus act on it), keeping a multi-selection
            // that already contains it.
            if (e.button == PointerButton::Right && i >= 0 && !is_selected((size_t)i)) click_select((size_t)i, 0);
            if (e.button != PointerButton::Left) return true;
            if (i < 0) { clear_selection(); return true; }
            if (model_->row_pressed((size_t)i, {e.pos.x, e.pos.y - i * row_height()}, e)) return true;
            if (e.click_count >= 2) { activated.emit((size_t)i); return true; }
            click_select((size_t)i, e.mods);
            return true;
        default:
            return true;
    }
}

void ListView::move_cursor(long to, uint32_t mods) {
    long n = (long)count();
    if (n == 0) return;
    to = std::clamp(to, 0L, n - 1);
    if (mode_ == SelectionMode::Multiple && (mods & MOD_SHIFT)) {
        select_range(anchor_, (size_t)to, false);
        selection_changed.emit();
    } else if (!(mods & MOD_CTRL) && mode_ != SelectionMode::None && model_->row_enabled((size_t)to)) {
        std::fill(selected_.begin(), selected_.end(), false);
        selected_[(size_t)to] = true;
        anchor_ = (size_t)to;
        selection_changed.emit();
    }
    set_current((size_t)to);
}

bool ListView::on_key(const KeyEvent& e) {
    if (!e.pressed || !model_ || count() == 0) return false;
    if (e.mods & MOD_ALT) return false;   // Alt+arrows belong to the owner (e.g. history navigation)
    long page = std::max(1L, (long)(visible_rect().height / row_height()) - 1);
    long c = (long)cursor_;
    switch (e.keysym) {
        case key::Up:       move_cursor(c - 1, e.mods); return true;
        case key::Down:     move_cursor(c + 1, e.mods); return true;
        case key::PageUp:   move_cursor(c - page, e.mods); return true;
        case key::PageDown: move_cursor(c + page, e.mods); return true;
        case key::Home:     move_cursor(0, e.mods); return true;
        case key::End:      move_cursor((long)count() - 1, e.mods); return true;
        case key::Return: case kKpEnter: activated.emit(cursor_); return true;
        case kSpace:
            if (mode_ == SelectionMode::Multiple && (e.mods & MOD_CTRL)) {
                selected_[cursor_] = !selected_[cursor_] && model_->row_enabled(cursor_);
                invalidate();
                selection_changed.emit();
                return true;
            }
            break;
    }
    if ((e.mods & MOD_CTRL) && e.keysym == key::letter('a')) { select_all(); return true; }
    return false;
}

bool ListView::on_text(const TextEvent& e) {
    if (!model_ || e.utf8.empty() || e.utf8 == " ") return false;
    // Type-ahead: jump to the next row starting with the typed prefix (resets after a pause).
    auto now = std::chrono::steady_clock::now();
    if (now - typeahead_time_ > std::chrono::milliseconds(900)) typeahead_.clear();
    typeahead_time_ = now;
    typeahead_ += lower(e.utf8);
    size_t n = count();
    size_t start = typeahead_.size() == e.utf8.size() ? cursor_ + 1 : cursor_;   // new search: from the next row
    for (size_t k = 0; k < n; ++k) {
        size_t i = (start + k) % n;
        if (lower(model_->row_text(i)).rfind(typeahead_, 0) == 0) { move_cursor((long)i, 0); return true; }
    }
    return true;
}

} // namespace uitk
