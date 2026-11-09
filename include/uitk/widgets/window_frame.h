// WindowFrame: the client-side window frame around the widget tree.
//   - transparent margin with a soft two-layer shadow (cross-fades with window focus)
//   - rounded corners (square when maximized/tiled)
//   - resize edges: hit testing in the border and in a grip just outside the edge
#pragma once
#include "uitk/core/geometry.h"
#include "uitk/platform/window.h"
#include "uitk/render/canvas.h"
#include "uitk/widgets/theme.h"

namespace uitk {

class WindowFrame {
public:
    static constexpr float BORDER      = 5;   // resize grip inside the window edge
    static constexpr float RESIZE_GRIP = 8;   // resize grip outside the edge (in the shadow)
    // Must contain the largest blur (3 sigma + offset) so the shadow fades out before the edge.
    static constexpr Insets shadow_insets() { return {24, 20, 24, 28}; }

    explicit WindowFrame(Window* window);   // null: offscreen

    void set_shadow(bool on);
    bool shadow() const { return shadow_; }
    void set_maximized(bool m) { maximized_ = m; }
    bool maximized() const { return maximized_; }

    // Resize edges (ResizeEdge bits) under `p` (window coordinates), 0 if none.
    unsigned resize_edges(Point p, Size window_size, const Theme& theme) const;
    static MouseCursor cursor_for_edges(unsigned edges);

    // begin(): shadow, rounded clip, background. end(): drop the clip. Content goes in between.
    void begin(Canvas& c, Size window_size, const Theme& theme, float window_active);
    void end(Canvas& c);

private:
    Window* window_;
    bool shadow_ = true;
    bool maximized_ = false;
};

} // namespace uitk
