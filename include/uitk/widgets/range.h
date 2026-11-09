// Range and activity controls: Slider, ProgressBar, Spinner.
#pragma once
#include "uitk/core/animation.h"
#include "uitk/core/signal.h"
#include "uitk/widgets/widget.h"

namespace uitk {

class Slider : public Widget {
public:
    Slider(float min = 0, float max = 100, float value = 0, float step = 1);

    Signal<float> changed;
    void set_value(float v);          // clamped and snapped to the step
    float value() const { return value_; }
    void set_range(float min, float max);
    void set_step(float step) { step_ = step; }
    float fraction() const { return max_ > min_ ? (value_ - min_) / (max_ - min_) : 0; }

    bool accepts_pointer() const override { return true; }
    bool focusable() const override { return true; }
    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;
    void paint_over(PaintContext& ctx) override;
    bool on_pointer(const PointerEvent& e) override;
    bool on_key(const KeyEvent& e) override;

protected:
    void on_state_changed(uint32_t) override { layer_.update(state() | (dragging_ ? STATE_PRESSED : 0u), theme()); }

private:
    Rect track_rect(const Theme& t) const;
    void set_from_x(float x);

    float min_, max_, value_, step_;
    bool dragging_ = false;
    StateLayer layer_;
};

class ProgressBar : public Widget {
public:
    explicit ProgressBar(float fraction = 0);
    void set_fraction(float f);              // 0..1, animated
    float fraction() const { return target_; }
    void set_indeterminate(bool on);         // endless sweep while visible
    bool indeterminate() const { return loop_.running(); }

    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;

private:
    float target_ = 0;
    Animated<float> shown_{0.f};
    Looping loop_{Millis(1400)};
};

class Spinner : public Widget {
public:
    explicit Spinner(float size = 0 /* theme icon size */);
    void start() { loop_.start(); invalidate(); }
    void stop() { loop_.stop(); invalidate(); }
    bool spinning() const { return loop_.running(); }

    Size measure(float available_width, const Theme& theme) override;
    void paint(PaintContext& ctx) override;

private:
    float size_;
    Looping loop_{Millis(900)};
};

} // namespace uitk
