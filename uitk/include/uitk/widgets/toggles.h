// Two-state controls.
//
//   ToggleControl  shared base: indicator + label, click/Space/Enter, animated checked state,
//                  focus ring around the indicator. Subclasses only draw their indicator.
//   CheckBox       square box with a check mark (optionally indeterminate)
//   RadioButton    round; exclusive within a RadioGroup
//   Switch         sliding knob on a track
//   ToggleButton   a Button that stays pressed while checked (also used for tabs)
#pragma once
#include <string>
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/button.h"
#include "uitk/widgets/label.h"

namespace uitk {

class ToggleControl : public Widget {
public:
    explicit ToggleControl(std::string label = {});

    Signal<bool> toggled;   // after every change of checked()

    void set_checked(bool checked);
    bool checked() const { return state() & STATE_CHECKED; }
    void set_label(std::string text) { label_->set_text(std::move(text)); label_->set_visible(!label_->text().empty()); }
    Label& label() { return *label_; }

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    Size measure(float available_width, const Theme& theme) override;
    void layout(const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    void paint_over(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;

protected:
    void on_state_changed(uint32_t old_state) override;
    virtual void activate() { set_checked(!checked()); }   // what a click does
    virtual Size indicator_size(const Theme& t) const = 0;
    virtual float indicator_radius(const Theme& t) const = 0;   // for the focus ring
    virtual void paint_indicator(PaintContext& ctx, const Rect& r, const StateLayer::Sample& s) = 0;
    Rect indicator_rect() const { return indicator_; }

private:
    Label* label_;
    StateLayer layer_;
    Rect indicator_;
};

class CheckBox : public ToggleControl {
public:
    using ToggleControl::ToggleControl;
    void set_indeterminate(bool on) { if (on != indeterminate_) { indeterminate_ = on; invalidate(); } }
    bool indeterminate() const { return indeterminate_; }

protected:
    void activate() override { indeterminate_ = false; ToggleControl::activate(); }
    Size indicator_size(const Theme& t) const override { return {t.check_size, t.check_size}; }
    float indicator_radius(const Theme& t) const override { return t.radius - 1; }
    void paint_indicator(PaintContext& ctx, const Rect& r, const StateLayer::Sample& s) override;

private:
    bool indeterminate_ = false;
};

class RadioButton;

// Keeps one RadioButton checked at a time. Owned by the application; must outlive its buttons.
class RadioGroup {
public:
    Signal<int> changed;   // index of the newly selected button
    int selected() const;
    void select(int index);

private:
    friend class RadioButton;
    std::vector<RadioButton*> buttons_;
    void select(RadioButton* b);
};

class RadioButton : public ToggleControl {
public:
    RadioButton(std::string label, RadioGroup& group);
    ~RadioButton() override;

protected:
    void activate() override;   // selects; never unchecks
    Size indicator_size(const Theme& t) const override { return {t.check_size, t.check_size}; }
    float indicator_radius(const Theme& t) const override { return t.check_size / 2; }
    void paint_indicator(PaintContext& ctx, const Rect& r, const StateLayer::Sample& s) override;

private:
    friend class RadioGroup;
    RadioGroup* group_;
};

class Switch : public ToggleControl {
public:
    using ToggleControl::ToggleControl;

protected:
    Size indicator_size(const Theme& t) const override { return {t.switch_width, t.switch_height}; }
    float indicator_radius(const Theme& t) const override { return t.switch_height / 2; }
    void paint_indicator(PaintContext& ctx, const Rect& r, const StateLayer::Sample& s) override;
};

class ToggleButton : public Button {
public:
    explicit ToggleButton(std::string label = {}, ButtonStyle style = ButtonStyle::Secondary);
    explicit ToggleButton(Image icon, ButtonStyle style = ButtonStyle::Flat);

    Signal<bool> toggled;
    void set_checked(bool checked);
    bool checked() const { return state() & STATE_CHECKED; }

    void paint_over(PaintContext& ctx) override;

protected:
    ButtonLook look(const Theme& t) const override;
};

} // namespace uitk
