#include "uitk/widgets/box.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace uitk {

namespace {
constexpr float kUnbounded = 1e9f;

// Place an item of natural length `natural` inside [start, start+avail] per alignment.
void place(Align a, float start, float avail, float natural, float& pos, float& len) {
    switch (a) {
        case Align::Fill:   pos = start; len = avail; break;
        case Align::Start:  pos = start; len = std::min(natural, avail); break;
        case Align::Center: len = std::min(natural, avail); pos = start + (avail - len) / 2; break;
        case Align::End:    len = std::min(natural, avail); pos = start + avail - len; break;
    }
}
} // namespace

Box::Box(Orientation o, float spacing) : orientation_(o), spacing_(spacing) {}

// Main-axis allocation shared by measure() and layout(): natural lengths plus an equal share
// of the leftover space for expanding children. For rows, children are then measured again at
// their allocated width (height-for-width: wrapping text gets taller when narrower).
Box::Allocation Box::allocate(float inner_main, float inner_cross, const Theme& t) {
    const bool vertical = orientation_ == Orientation::Vertical;
    Allocation a;
    float total = 0;
    int expanders = 0;
    for (auto& c : children()) {
        if (!c->visible()) continue;
        Size s = c->measure_clamped(vertical ? inner_cross : kUnbounded, t);
        a.items.push_back({c.get(), s, vertical ? s.height : s.width});
        total += vertical ? s.height : s.width;
        if (vertical ? c->vexpand() : c->hexpand()) ++expanders;
    }
    a.gap = spacing(t);
    if (a.items.empty()) return a;
    total += a.gap * (a.items.size() - 1);
    a.extra = inner_main >= kUnbounded / 2 ? 0 : std::max(0.f, inner_main - total);
    a.expanders = expanders;
    if (expanders) {
        float share = a.extra / expanders;
        for (auto& it : a.items)
            if (vertical ? it.w->vexpand() : it.w->hexpand()) it.length += share;
    }
    // Shrink: on overflow, take the excess from expanding children (e.g. a ScrollView in a
    // column gets the remaining height and scrolls, instead of growing past the window).
    if (inner_main < kUnbounded / 2 && total > inner_main && expanders) {
        float over = (total - inner_main) / expanders;
        for (auto& it : a.items)
            if (vertical ? it.w->vexpand() : it.w->hexpand()) it.length = std::max(0.f, it.length - over);
    }
    if (!vertical)   // height-for-width
        for (auto& it : a.items) it.natural.height = it.w->measure_clamped(it.length, t).height;
    return a;
}

Size Box::measure(float available_width, const Theme& t) {
    const bool vertical = orientation_ == Orientation::Vertical;
    float inner_w = std::max(0.f, available_width - padding_.left - padding_.right);
    Allocation a = allocate(vertical ? kUnbounded : inner_w, inner_w, t);
    float main = 0, cross = 0;
    for (auto& it : a.items) {
        main += vertical ? it.natural.height : it.length;
        cross = std::max(cross, vertical ? it.natural.width : it.natural.height);
    }
    if (!vertical) {   // a row's natural width is the sum of natural widths, not the allocation
        main = 0;
        for (auto& it : a.items) main += it.natural.width;
    }
    if (a.items.size() > 1) main += a.gap * (a.items.size() - 1);
    return vertical ? Size{cross + padding_.left + padding_.right, main + padding_.top + padding_.bottom}
                    : Size{main + padding_.left + padding_.right, cross + padding_.top + padding_.bottom};
}

void Box::layout(const Theme& t) {
    const bool vertical = orientation_ == Orientation::Vertical;
    Rect inner{padding_.left, padding_.top,
               std::max(0.f, size().width - padding_.left - padding_.right),
               std::max(0.f, size().height - padding_.top - padding_.bottom)};
    float main_avail = vertical ? inner.height : inner.width;
    float cross_avail = vertical ? inner.width : inner.height;
    Allocation a = allocate(main_avail, cross_avail, t);
    if (a.items.empty()) return;

    float gap = a.gap;
    float cursor = vertical ? inner.y : inner.x;
    if (!a.expanders) {
        if (justify_ == Justify::Center) cursor += a.extra / 2;
        else if (justify_ == Justify::End) cursor += a.extra;
        else if (justify_ == Justify::SpaceBetween && a.items.size() > 1) gap += a.extra / (a.items.size() - 1);
    }

    for (auto& it : a.items) {
        float len = it.length;
        float cpos, clen;
        if (vertical) place(it.w->halign(), inner.x, cross_avail, it.natural.width, cpos, clen);
        else          place(it.w->valign(), inner.y, cross_avail, it.natural.height, cpos, clen);
        // Snap to whole logical pixels: crisp edges, no shimmering text.
        Rect r = vertical ? Rect{std::round(cpos), std::round(cursor), std::round(clen), std::round(len)}
                          : Rect{std::round(cursor), std::round(cpos), std::round(len), std::round(clen)};
        it.w->set_bounds(r);
        it.w->layout(t);
        cursor += len + gap;
    }
}

} // namespace uitk
