// Expander: a header that shows or hides its content (collapsible section).
// Accordion: a column of Expanders (VS Code side bar sections); optionally only one open.
//
//   auto& ex = box.add<Expander>("Advanced", false);
//   ex.content().add<CheckBox>("Verbose logging");
//   ex.header_actions().add<Button>(icons::symbolic("view-refresh"));   // shown on the right
//
// The content height and the chevron animate; click, Space or Enter on the header toggles,
// Left/Right collapse/expand. Collapsed content is out of the Tab chain.
#pragma once
#include <string>

#include "uitk/core/animation.h"
#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/label.h"

namespace uitk {

enum class ExpanderStyle {
    Plain,     // chevron + title (forms, settings)
    Section,   // VS Code side bar section: small bold upper-case title, full-width header
};

class Expander : public Widget {
public:
    explicit Expander(std::string title, bool expanded = false, ExpanderStyle style = ExpanderStyle::Plain);

    Box& content() { return *content_; }                    // vertical box (padding settable)
    template <class W, class... Args>
    W& set_content(Args&&... args) {                         // a single content widget instead
        content_->clear_children();
        W& w = content_->add<W>(std::forward<Args>(args)...);
        w.set_expand(true, true);
        return w;
    }
    Box& header_actions() { return *actions_; }
    void set_title(std::string t);
    void set_expanded(bool expanded, bool animate = true);
    bool expanded() const { return expanded_; }
    // When expanded, take the extra space of the parent Box (side bar sections with lists).
    void set_fill(bool fill) { fill_ = fill; set_expand(hexpand(), fill_ && expanded_); }

    Signal<bool> toggled;

    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    void paint(PaintContext& ctx) override;

protected:
    void on_state_changed(uint32_t) override { update_actions(); }

private:
    class Header;
    friend class Header;
    float progress() const;   // 0 collapsed .. 1 expanded (animated)
    void update_actions();

    Header* header_;
    Label* title_;
    Box* actions_;
    Box* content_;
    ExpanderStyle style_;
    bool expanded_;
    bool fill_ = false;
    Animated<float> anim_;
};

class Accordion : public Box {
public:
    Accordion();
    Expander& add_section(std::string title, bool expanded = false);
    // Only one section open at a time (opening one closes the others).
    void set_exclusive(bool e) { exclusive_ = e; }

private:
    bool exclusive_ = false;
    std::vector<Expander*> sections_;
};

} // namespace uitk
