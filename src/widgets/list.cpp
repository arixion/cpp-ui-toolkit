#include "uitk/widgets/list.h"

#include <algorithm>

#include "uitk/widgets/widget_host.h"

namespace uitk {

// ---------------------------------------------------------------------------- ListRow

ListRow::ListRow(std::string title, Image icon) : Box(Orientation::Horizontal, 6.f) {
    set_padding(Insets{8, 0, 4, 0});
    set_align(Align::Fill, Align::Start);
    bool has_icon = icon.valid();
    icon_ = &add<Icon>(std::move(icon));
    icon_->set_visible(has_icon);
    title_ = &add<Label>(std::move(title));
    title_->set_align(Align::Start, Align::Center);
    detail_icon_ = &add<Icon>();
    detail_icon_->set_visible(false);
    detail_icon_->set_size(14);
    detail_icon_->set_token(ColorToken::ForegroundDim);
    detail_ = &add<Label>("", TextRole::Dim);
    detail_->set_align(Align::Start, Align::Center);
    detail_->set_visible(false);
    add<Spacer>().set_expand(true, false);
    actions_ = &add<Box>(Orientation::Horizontal, 2.f);
    actions_->set_align(Align::Start, Align::Center);
    actions_->set_visible(false);
}

void ListRow::set_detail(std::string text, Image icon) {
    detail_->set_text(std::move(text));
    detail_->set_visible(!detail_->text().empty());
    bool has_icon = icon.valid();
    detail_icon_->set_image(std::move(icon));
    detail_icon_->set_visible(has_icon);
}

Size ListRow::measure(float available_width, const Theme& t) {
    Size s = Box::measure(available_width, t);
    return {s.width, std::max(s.height, t.list_row_height)};
}

void ListRow::set_checked(bool on) { set_checked_state(on); }

void ListRow::set_selected(bool sel) {
    if (auto* box = dynamic_cast<ListBox*>(parent())) box->select(sel ? this : (box->selected() == this ? nullptr : box->selected()));
    else set_checked(sel);
}

void ListRow::on_state_changed(uint32_t) {
    layer_.update(state(), theme());
    update_content();
}

void ListRow::update_content() {
    bool sel = selected();
    // Selection recolours the content (tokens: resolved at paint, so theme changes just work);
    // actions appear while the row is engaged.
    title_->set_color_token(sel ? std::optional<ColorToken>(ColorToken::SelectionFg) : std::nullopt);
    detail_->set_color_token(sel ? std::optional<ColorToken>(ColorToken::SelectionFgDim) : std::nullopt);
    detail_icon_->set_token(sel ? ColorToken::SelectionFgDim : ColorToken::ForegroundDim);
    icon_->set_token(sel ? ColorToken::SelectionFg : ColorToken::Foreground);
    bool show_actions = state() & (STATE_HOVER_WITHIN | STATE_CHECKED | STATE_FOCUSED);
    if (actions_->visible() != show_actions && !actions_->children().empty()) actions_->set_visible(show_actions);
}

void ListRow::paint(PaintContext& ctx) {
    const Theme& t = ctx.theme;
    StateLayer::Sample s = layer_.sample(ctx.clock);
    // Selected rows: selection background; the hover overlay still applies on top.
    Color base = lerp(Color::transparent(), t.selection_bg, s.checked);
    Color fill = StateLayer::surface(base, t.button_flat.overlay, s, t);
    if (fill.a) ctx.canvas.draw_rrect(local_rect(), t.list_radius, Paint::fill(fill));
}

void ListRow::paint_over(PaintContext& ctx) {
    StateLayer::paint_focus_ring(ctx.canvas, local_rect().inset(1, 1), ctx.theme.list_radius, layer_.sample(ctx.clock), ctx.theme);
}

bool ListRow::on_pointer(const PointerEvent& e) {
    if (e.button != PointerButton::Left) return e.type == PointerEvent::Type::Down;
    if (e.type == PointerEvent::Type::Down) set_selected(true);
    if (e.type == PointerEvent::Type::Up && local_rect().contains(e.pos)) activated.emit();
    return true;
}

bool ListRow::on_key(const KeyEvent& e) {
    if (e.pressed && (e.keysym == key::Return || e.keysym == 0x20)) {
        set_selected(true);
        activated.emit();
        return true;
    }
    return false;   // navigation keys bubble to the ListBox
}

// ---------------------------------------------------------------------------- ListBox

ListBox::ListBox() : Box(Orientation::Vertical, 1.f) {}

void ListBox::select(ListRow* row) {
    if (row == selected_) return;
    if (selected_) selected_->set_checked(false);
    selected_ = row;
    if (row) {
        row->set_checked(true);
        selection_changed.emit(*row);
    }
}

bool ListBox::on_key(const KeyEvent& e) {
    if (!e.pressed) return false;
    std::vector<ListRow*> rows;
    for (auto& c : children())
        if (auto* r = dynamic_cast<ListRow*>(c.get()); r && r->visible() && r->enabled()) rows.push_back(r);
    if (rows.empty()) return false;
    WidgetHost* h = host();
    ListRow* current = h ? dynamic_cast<ListRow*>(h->focus()) : nullptr;
    if (!current) current = selected_;
    long i = current ? (long)(std::find(rows.begin(), rows.end(), current) - rows.begin()) : -1;
    long n = (long)rows.size();
    long next;
    switch (e.keysym) {
        case key::Up:   next = i <= 0 ? 0 : i - 1; break;
        case key::Down: next = i < 0 ? 0 : std::min(n - 1, i + 1); break;
        case key::Home: next = 0; break;
        case key::End:  next = n - 1; break;
        default: return false;
    }
    select(rows[(size_t)next]);
    if (h) h->set_focus(rows[(size_t)next], true);
    return true;
}

} // namespace uitk
