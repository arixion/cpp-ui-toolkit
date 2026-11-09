// Containers and small display widgets: ScrollBar, ScrollView, TabView, Badge.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "uitk/core/animation.h"
#include "uitk/core/signal.h"
#include "uitk/widgets/box.h"
#include "uitk/widgets/toggles.h"

namespace uitk {

// Overlay scrollbar (VS Code style): thin thumb, shown while hovered or scrolling.
class ScrollBar : public Widget {
public:
    explicit ScrollBar(Orientation o = Orientation::Vertical);
    Signal<float> scrolled;   // requested offset

    void set_metrics(float content, float viewport, float offset);
    void flash();             // show briefly (after scrolling)
    bool needed() const { return content_ > viewport_ + 0.5f; }

    bool accepts_pointer() const override { return needed(); }
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;

protected:
    void on_state_changed(uint32_t) override { layer_.update(state() | (dragging_ ? STATE_PRESSED : 0u), theme()); }

private:
    Rect thumb() const;
    bool vertical() const { return orientation_ == Orientation::Vertical; }

    Orientation orientation_;
    float content_ = 0, viewport_ = 0, offset_ = 0;
    bool dragging_ = false;
    float grab_ = 0;   // pointer offset inside the thumb while dragging
    StateLayer layer_;
    Animated<float> visible_{0.f};
    TimePoint last_activity_{};
};

// Scrolls one content widget vertically. Wheel scrolling is spring-smoothed; keyboard focus
// inside is scrolled into view automatically.
class ScrollView : public Widget {
public:
    ScrollView();

    template <class W, class... Args>
    W& set_content(Args&&... args) {
        if (content_) remove_child(content_);
        W& w = insert<W>(0, std::forward<Args>(args)...);
        content_ = &w;
        return w;
    }
    Widget* content() const { return content_; }

    void scroll_to(float offset, bool animate = true);
    float offset() const { return target_; }
    float max_offset() const { return std::max(0.f, content_height_ - size().height); }

    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    bool on_scroll(const ScrollEvent& e) override;
    bool on_key(const KeyEvent& e) override;
    void reveal_area(Widget* from, const Rect& area) override;

private:
    void apply_offset(float y);

    Widget* content_ = nullptr;
    ScrollBar* bar_;
    Spring spring_{0.f, SpringParams::critical(520)};
    float target_ = 0, shown_ = 0, content_height_ = 0;
};

// Tabs over pages; tabs are ToggleButtons with an animated indicator underneath.
class TabView : public Widget {
public:
    TabView();
    Signal<int> changed;

    template <class W = Box, class... Args>
    W& add_tab(std::string title, Args&&... args) {
        W& page = pages_->add<W>(std::forward<Args>(args)...);
        add_tab_button(std::move(title), &page);
        return page;
    }
    void set_current(int index);
    int current() const { return current_; }
    int count() const { return (int)tabs_.size(); }

    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    void paint_over(PaintContext& ctx) override;
    bool on_key(const KeyEvent& e) override;

private:
    void add_tab_button(std::string title, Widget* page);

    Box* strip_;
    Widget* pages_;
    std::vector<ToggleButton*> tabs_;
    std::vector<Widget*> page_list_;
    int current_ = -1;
    Animated<Rect> indicator_;
};

// Small pill with a count or short status ("3", "New").
class Badge : public Widget {
public:
    explicit Badge(std::string text);
    void set_text(std::string t) { if (t != text_) { text_ = std::move(t); invalidate_layout(); } }
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;

private:
    std::string text_;
};

} // namespace uitk
