#include "uitk/widgets/expander.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "uitk/widgets/glyphs.h"
#include "uitk/widgets/widget_host.h"

namespace uitk {

namespace {
constexpr uint32_t kSpace = 0x20, kKpEnter = 0xff8d;
constexpr float kChevron = 20;
const Transition kExpand{Millis(180), Easing::ease_out_cubic()};

std::string upper(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
} // namespace

// Clickable, focusable header row: chevron (painted) + title + actions.
class Expander::Header : public Box {
public:
    explicit Header(Expander& owner) : Box(Orientation::Horizontal, 4.f), owner_(owner) {}
    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }

    void paint(PaintContext& ctx) override {
        const Theme& t = ctx.theme;
        StateLayer::Sample s = layer_.sample(ctx.clock);
        Rect r = local_rect();
        bool section = owner_.style_ == ExpanderStyle::Section;
        Color bg = StateLayer::surface(Color::transparent(), t.button_flat.overlay, s, t);
        if (bg.a) ctx.canvas.draw_rrect(r, section ? 0.f : t.radius, Paint::fill(bg));
        // Chevron: points right when collapsed, rotates down while expanding.
        float p = owner_.progress();
        Rect c{4, std::round((r.height - 16) / 2), 16, 16};
        ctx.canvas.save();
        ctx.canvas.rotate(90 * p, c.center());
        ctx.canvas.draw_image(glyph(Glyph::ChevronRight), c, StateLayer::content(t.foreground, s, t));
        ctx.canvas.restore();
    }
    void paint_over(PaintContext& ctx) override {
        StateLayer::paint_focus_ring(ctx.canvas, local_rect().inset(1, 1), ctx.theme.radius, layer_.sample(ctx.clock), ctx.theme);
    }
    bool on_pointer(const PointerEvent& e) override {
        if (e.type == PointerEvent::Type::Down && e.button == PointerButton::Left) owner_.set_expanded(!owner_.expanded_);
        return e.type == PointerEvent::Type::Down || e.type == PointerEvent::Type::Up;
    }
    bool on_key(const KeyEvent& e) override {
        if (!e.pressed) return false;
        switch (e.keysym) {
            case kSpace: case key::Return: case kKpEnter: owner_.set_expanded(!owner_.expanded_); return true;
            case key::Left:  if (!owner_.expanded_) return false; owner_.set_expanded(false); return true;
            case key::Right: if (owner_.expanded_) return false; owner_.set_expanded(true); return true;
        }
        return false;
    }

protected:
    void on_state_changed(uint32_t) override { layer_.update(state(), theme()); owner_.update_actions(); }

private:
    Expander& owner_;
    StateLayer layer_;
};

Expander::Expander(std::string title, bool expanded, ExpanderStyle style)
    : style_(style), expanded_(expanded), anim_(expanded ? 1.f : 0.f) {
    set_clip_children(true);
    set_align(Align::Fill, Align::Start);
    header_ = &add<Header>(*this);
    header_->set_padding(Insets{kChevron + 4, 0, 4, 0});
    title_ = &header_->add<Label>("", style == ExpanderStyle::Section ? TextRole::Caption : TextRole::Body);
    title_->set_align(Align::Start, Align::Center);
    title_->set_expand(true, false);
    title_->set_ellipsize(true);
    actions_ = &header_->add<Box>(Orientation::Horizontal, 0.f);
    actions_->set_align(Align::End, Align::Center);
    if (style == ExpanderStyle::Section) actions_->set_visible(false);
    content_ = &add<Box>(Orientation::Vertical, 6.f);
    content_->set_padding(style == ExpanderStyle::Section ? Insets{0, 0, 0, 0} : Insets{kChevron + 4, 4, 4, 8});
    content_->set_visible(expanded);
    set_title(std::move(title));
}

void Expander::set_title(std::string t) {
    title_->set_text(style_ == ExpanderStyle::Section ? upper(std::move(t)) : std::move(t));
}

void Expander::update_actions() {
    if (style_ != ExpanderStyle::Section) return;
    // Section actions show while the section is hovered or keyboard focus is in its header.
    WidgetHost* h = host();
    bool focus_in = h && h->focus() && header_->is_ancestor_of(h->focus());
    actions_->set_visible((state() & STATE_HOVER_WITHIN) || focus_in);
}

float Expander::progress() const {
    if (WidgetHost* h = host()) return anim_.value(h->clock());
    return anim_.target();
}

void Expander::set_expanded(bool e, bool animate) {
    if (e == expanded_) return;
    expanded_ = e;
    if (animate) anim_.set(e ? 1.f : 0.f, kExpand); else anim_.jump(e ? 1.f : 0.f);
    if (e) content_->set_visible(true);   // collapsing hides it when the animation ends
    else if (WidgetHost* h = host(); h && h->focus() && content_->is_ancestor_of(h->focus())) header_->grab_focus();
    set_expand(hexpand(), fill_ && expanded_);
    invalidate_layout();
    toggled.emit(e);
}

Size Expander::measure(float available_width, const Theme& t) {
    Size hs = header_->measure_clamped(available_width, t);
    float hh = std::max(hs.height, t.list_row_height);
    float p = progress();
    Size cs = (content_->visible() && p > 0) ? content_->measure_clamped(available_width, t) : Size{};
    return {std::max(hs.width + kChevron, cs.width), hh + std::round(cs.height * p)};
}

void Expander::layout(const Theme& t) {
    float w = size().width;
    float hh = std::max(header_->measure_clamped(w, t).height, t.list_row_height);
    header_->set_bounds({0, 0, w, hh});
    header_->layout(t);
    if (content_->visible()) {
        // Full content height (clipped while animating); a filling section gets the rest.
        float ch = content_->measure_clamped(w, t).height;
        if (fill_ && expanded_) ch = std::max(ch, size().height - hh);
        content_->set_bounds({0, hh, w, ch});
        content_->layout(t);
    }
}

void Expander::paint(PaintContext& ctx) {
    if (anim_.running(ctx.clock)) {
        anim_.value(ctx.clock);   // keeps frames coming
        invalidate_layout();      // height follows the animation
    } else if (!expanded_ && content_->visible()) {
        content_->set_visible(false);   // collapse finished
        invalidate_layout();
    }
}

// ---------------------------------------------------------------------------- Accordion

Accordion::Accordion() : Box(Orientation::Vertical, 0.f) {}

Expander& Accordion::add_section(std::string title, bool expanded) {
    Expander& ex = add<Expander>(std::move(title), expanded, ExpanderStyle::Section);
    sections_.push_back(&ex);
    ex.toggled.connect([this, &ex](bool on) {
        if (!on || !exclusive_) return;
        for (Expander* o : sections_) if (o != &ex) o->set_expanded(false);
    });
    return ex;
}

} // namespace uitk
