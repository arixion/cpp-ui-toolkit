#pragma once
#include <cstdint>

namespace uitk {

// Straight (non-premultiplied) 8-bit RGBA colour.
struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) { return {r, g, b, 255}; }
    static constexpr Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return {r, g, b, a}; }
    // 0xRRGGBB
    static constexpr Color hex(uint32_t v) {
        return {uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 255};
    }
    static constexpr Color transparent() { return {0, 0, 0, 0}; }

    constexpr Color with_alpha(uint8_t alpha) const { return {r, g, b, alpha}; }
    // Alpha as a fraction 0..1 (e.g. "foreground at 10%").
    constexpr Color with_opacity(float opacity) const {
        float o = opacity < 0 ? 0 : opacity > 1 ? 1 : opacity;
        return {r, g, b, uint8_t(o * 255.f + 0.5f)};
    }
    // Multiply the existing alpha (fades, disabled states).
    constexpr Color faded(float factor) const {
        float f = factor < 0 ? 0 : factor > 1 ? 1 : factor;
        return {r, g, b, uint8_t(a * f + 0.5f)};
    }
    // Source-over compositing of `top` onto `bottom` (straight alpha in, straight alpha out):
    // flattens state overlays onto a base colour without a canvas layer.
    static constexpr Color over(Color top, Color bottom) {
        float ta = top.a / 255.f, ba = bottom.a / 255.f;
        float oa = ta + ba * (1 - ta);
        if (oa <= 0) return transparent();
        auto ch = [&](uint8_t t, uint8_t b) {
            return uint8_t((t * ta + b * ba * (1 - ta)) / oa + 0.5f);
        };
        return {ch(top.r, bottom.r), ch(top.g, bottom.g), ch(top.b, bottom.b), uint8_t(oa * 255.f + 0.5f)};
    }
    constexpr uint32_t argb() const {
        return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }
    friend constexpr bool operator==(Color, Color) = default;
};

} // namespace uitk
