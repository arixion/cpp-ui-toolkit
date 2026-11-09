// Interaction state shared by every interactive control.
//
// WidgetState flags describe what is happening (hovered, pressed, focused...). StateLayer turns
// flag changes into animated 0..1 values using the theme's motion tokens, and provides the
// standard colour treatment (content-coloured overlays on a base fill, disabled fading) and the
// focus ring. Controls compose a StateLayer instead of re-implementing hover/press logic.
#pragma once
#include <cstdint>

#include "uitk/core/animation.h"
#include "uitk/render/canvas.h"
#include "uitk/widgets/theme.h"

namespace uitk {

enum WidgetState : uint32_t {
    STATE_NONE          = 0,
    STATE_HOVERED       = 1u << 0,
    STATE_PRESSED       = 1u << 1,
    STATE_FOCUSED       = 1u << 2,
    STATE_FOCUS_VISIBLE = 1u << 3,   // focused via keyboard: show the focus ring
    STATE_DISABLED      = 1u << 4,
    STATE_CHECKED       = 1u << 5,   // toggles, check boxes, selected rows
    STATE_HOVER_WITHIN  = 1u << 6,   // the pointer is over this widget or one of its descendants
};

class StateLayer {
public:
    // Snapshot of the animated state for one frame.
    struct Sample {
        float hover = 0, press = 0, focus_ring = 0, disabled = 0, checked = 0;
    };

    // Retarget animations for a new flag set (call from Widget::on_state_changed).
    void update(uint32_t state, const Theme& theme);
    // Set without animation (initial state, offscreen rendering).
    void jump(uint32_t state);
    Sample sample(FrameClock& clock) const;

    // ---- standard treatments ----
    // Base fill tinted with `content` by the hover/press overlays, faded when disabled.
    static Color surface(Color base, Color content, const Sample& s, const Theme& theme);
    // Content (text/icon) colour faded when disabled.
    static Color content(Color c, const Sample& s, const Theme& theme);
    // Keyboard focus ring around `r` (outside it), animated in/out.
    static void paint_focus_ring(Canvas& canvas, const Rect& r, float radius, const Sample& s,
                                 const Theme& theme);

private:
    uint32_t state_ = 0;
    bool initialized_ = false;   // the first update jumps (no fade-in of the initial state)
    Animated<float> hover_, press_, focus_, disabled_, checked_;
};

} // namespace uitk
