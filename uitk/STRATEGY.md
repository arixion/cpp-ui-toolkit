# Cross-Platform C++ UI Toolkit — Strategy

## 1. Goals & Non-Goals

**Goals**
- Custom-drawn UI: every pixel is produced by us, not the OS toolkit.
- Cross-platform: Windows, macOS, Linux (x64/arm64).
- Window chrome owned by us: no native title bar, custom drop shadow, rounded corners, snap-to-screen-edge behavior.
- Rendering via Skia (C++), with a thin platform glue layer per OS.
- Single codebase for layout, components, text, effects.

**Non-Goals (v1)**
- Native accessibility tree parity (we'll expose a minimal subset).
- OS-native IME beyond basic text input.
- Mobile/web targets.

## 2. High-Level Architecture

```
┌─────────────────────────────────────────────┐
│  App / Components / Layout / Styling        │
├─────────────────────────────────────────────┤
│  Scene Graph + Render Tree (our own)        │
├─────────────────────────────────────────────┤
│  Skia Renderer (SkCanvas, SkSurface)        │
├─────────────────────────────────────────────┤
│  Platform Window Layer (per-OS)             │
│   - HWND / NSWindow / X11-Wayland          │
│   - Input events, DPI, IME                  │
├─────────────────────────────────────────────┤
│  OS Low-Level APIs                          │
│   - Drop shadow, rounded corners, snapping   │
└─────────────────────────────────────────────┘
```

## 3. Windowing Strategy (No Native Frame)

We create a **borderless, transparent, top-level window** and draw everything ourselves.

| OS | Technique |
|----|-----------|
| Windows | `WS_POPUP` + `WS_EX_LAYERED` + DWM `DwmExtendFrameIntoClientArea` / `DwmEnableBlurBehindWindow`; use `SetWindowRgn`/`UpdateLayeredWindow` for alpha; shadow via `DWMWINDOWATTRIBUTE` + `CS_DROPSHADOW` class style or `DwmSetWindowAttribute(DWMWA_NCRENDERING_POLICY)`. Snap-to-edge via `WM_WINDOWPOSCHANGING` + monitor work-area clamping. |
| macOS | `NSWindow` with `styleMask = .borderless`, `isOpaque = false`, `backgroundColor = .clear`, `hasShadow = true` (or custom `shadow`); rounded corners via layer `cornerRadius` + `masksToBounds`. Snap via `NSWindowDelegate` `windowWillMove`. |
| Linux/X11 | Override-redirect or `_NET_WM_WINDOW_TYPE_NORMAL` with `_MOTIF_WM_HINTS` (decorations off); compositor shadow via `_NET_WM_WINDOW_OPACITY`/`_GTK_FRAME_EXTENTS`. Wayland: xdg-toplevel with no decorations, compositor draws shadow. |

**Rounded corners**: render into an offscreen Skia surface with alpha, apply a rounded-clip path; the OS window itself stays rectangular but transparent regions let the desktop show through.

**Drop shadow**: prefer OS compositor shadow (DWM / macOS window shadow / compositor) for performance; fall back to drawing a blurred Skia shadow layer ourselves when compositor shadow is unavailable (e.g., some Linux WMs).

## 4. Rendering: Skia C++ (Approach A — Recommended)

**Approach A: Direct Skia C++**
- Link `libskia` (and `libskshaper`, `libskparagraph`, `libskunicode`) statically/dynamically.
- Create `GrDirectContext` (GPU) or raster `SkSurface` per window.
- Draw with `SkCanvas`: paths, text via SkShaper, images, filters.
- No JNI/interop overhead. Full control over threading and surfaces.

**Approach B: Skia via a generated binding layer (skiko-style)**
- Keep a C API shim (`extern "C"`) over Skia, consumed by higher-level language or by our own C++ via the same shim.
- Useful only if we later want Kotlin/Swift/JS front-ends. For a pure C++ toolkit, this adds indirection with no benefit.

**Decision**: Use Approach A directly. Skiko exists because Kotlin needs JNI; C++ does not. We can still expose a stable C ABI later for plugins without changing internals.

## 5. Text, Fonts, Input

- Text shaping: `SkShaper` + `SkFontMgr` (HarfBuzz underneath).
- Fonts: bundle a default (Inter) + system font fallback chain.
- Input: platform event loop → normalized `Event` struct (pointer, key, text, scroll, resize) → our widget tree.
- DPI: query per-monitor scale; scale all logical coords; re-rasterize on monitor change.

## 6. Layout & Styling

- Flexbox-like layout engine (Yoga or our own) operating on a node tree.
- Styling: a small CSS-like subset (box model, colors, borders, shadows, transforms) compiled to a style struct; no full CSS engine in v1.

## 7. Build System

- CMake + vcpkg (or Conan) for Skia, HarfBuzz, ICU, GLFW (only as reference, not runtime).
- Per-platform backends compiled conditionally (`#ifdef _WIN32`, `__APPLE__`, `__linux__`).
- CI: GitHub Actions matrix building static libs.

## 8. Milestones

1. **M0** — Window opens borderless, transparent, rounded, with OS drop shadow; clears to a color. (per OS)
2. **M1** — Skia canvas renders rectangles/text/images; resize + DPI.
3. **M2** — Input events reach a widget tree; basic button/label.
4. **M3** — Layout engine + theming.
5. **M4** — Advanced: animations, effects, accessibility basics.

## 9. Risks

- Wayland security model: no global positioning; snapping must be compositor-assisted.
- macOS notarization for custom frameworks.
- Skia build size (~30MB); consider `skia` minimal build flags.
- Text shaping correctness across scripts (use ICU).

## 10. Recommendation

Proceed with **Approach A (direct Skia C++)** + per-OS borderless window layer + OS-assisted shadow/rounding, falling back to Skia-drawn effects. Start with Windows backend to validate the chrome-less window + shadow + snapping, then port.

---
*Generated for cpp-ui-toolkit repository.*
