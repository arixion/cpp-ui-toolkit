---
name: build-and-run
description: Build the cpp-ui-toolkit libraries and examples (CPU and GPU render backends; Linux Wayland + X11, Windows Win32) with CMake/Ninja, run tests, launch the hello example, and render offscreen snapshots to PNG for visual checks. Use when asked to build, run, launch, rebuild, test, benchmark, or screenshot the app, to check the Windows build, or to confirm a change works.
---

# Build and run cpp-ui-toolkit (Linux; Windows in section 5)

Layout: `uitk_core`, `uitk_platform` (Wayland + X11, picked at runtime), `uitk_chrome`, and the two
render backends `uitk_render_cpu` (Skia raster only, no GPU code) and `uitk_render_gpu`
(Skia Ganesh on GLES via EGL, with a raster fallback). Each example or tool is built once per
backend: `hello_cpu`/`hello_gpu` and `uitk_snapshot_cpu`/`uitk_snapshot_gpu`.
Other examples, which are complex controls kept out of the hello gallery:
- `files_cpu`/`files_gpu`: file upload and file browser.
- `workbench_cpu`/`workbench_gpu`: an IDE-style window with menus, tree, table, dialogs and splitters.

Both accept `--dir PATH` and `--snapshot out.png [--dark]`. The workbench also accepts
`--menu`, `--context` and `--dialog`.

Use `build-linux/`. Don't use `build/`: it's a stale cache made under WSL.

## 1. Skia (one-time, ~15–30 min each)

```bash
ls third_party/skia/out/cpu/libskia.a third_party/skia/out/gpu/libskia.a   # already built?
tools/build_skia.sh cpu     # CPU-only Skia
tools/build_skia.sh gpu     # GPU Skia (only needed for UITK_RENDER_GPU=ON)
```
These builds are long: run them in the background and wait for the completion notification.
The script pins `chrome/m157` and uses the system freetype/harfbuzz/icu/png/jpeg/webp.

## 2. Build + test

```bash
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DUITK_RENDER_GPU=ON
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
```
Use `-DUITK_RENDER_GPU=OFF` if only the CPU Skia is built. Report any compiler error together
with its output, and don't call the build done if a target failed. `compile_commands.json` is
symlinked at the repo root for clangd. Without it, the editor shows false "file not found" errors.

## 3. Run

Stop old instances first with `kill $(pidof hello_cpu) $(pidof hello_gpu) 2>/dev/null`.
Use `pidof`, never `pgrep`/`pkill -f`: those match your own shell command.

```bash
LOG=<scratchpad>/hello.log
(./build-linux/hello_gpu > "$LOG" 2>&1 &)                         # native Wayland
(UITK_PLATFORM=x11 ./build-linux/hello_cpu > "$LOG.x11" 2>&1 &)   # force X11 (XWayland)
sleep 2; pidof hello_gpu hello_cpu; cat "$LOG"
```
The log line `hello: platform=<wayland|x11> renderer=<skia-raster | skia-ganesh-gles (<GPU>)>` means
it started. If the process exited, show the log.

Environment overrides:
- `UITK_PLATFORM=x11|wayland` picks the window system.
- `UITK_RENDERER=cpu` forces the GPU binary to use raster.
- `UITK_SCALE=2` sets the X11 HiDPI scale.
- `UITK_REDUCED_MOTION=1` turns animations off (transitions complete instantly).
- `UITK_NO_SHM=1` makes X11 present with plain `XPutImage` instead of MIT-SHM shared memory (for comparison or debugging).

Benchmark: `./build-linux/hello_gpu --bench 5` redraws continuously for 5 s and prints average fps.
stderr also shows the draw time in ms per frame. The fps is capped by the display (~60), so compare
the ms/frame numbers between backends.

Tell the user about these in-app controls: drag the title bar to move, drag the edges to resize,
double-click the title bar to maximize, right-click it for the window menu, **T** toggles the
theme, **F** toggles continuous redraw with an FPS readout, and the Adwaita buttons
minimize/maximize/close.

## 4. Visual check

GNOME blocks command-line screenshots of Wayland windows. Use these instead:
- **Offscreen snapshot** (exactly what the app paints):
  ```bash
  ./build-linux/uitk_snapshot_cpu <scratchpad>/s.png [--dark] [--scale 2] [--size 900x600] [--hover min|max|close] [--maximized]
  ```
  `uitk_snapshot_gpu` renders through headless EGL instead. To check the two backends match,
  `magick compare -metric AE -fuzz 2% a.png b.png diff.png`. Crop off the bottom info line first,
  because it prints the backend name.
- **Live X11 window**: start it with `UITK_PLATFORM=x11`, then
  `import -window "uitk hello (CPU)" <scratchpad>/live.png`. This works through XWayland.

Look at PNGs with the Read tool. Input handling (dragging, clicking, keys) can't be checked
from here, so ask the user to try it.

## 5. Windows

Code layout: `src/platform/windows/win32_window.cpp` (Win32 window; DWM shadow and rounded
corners, so `Window::native_shadow()` is true), `src/render/gpu/wgl_context.cpp` (GPU backend:
desktop GL via WGL), DirectWrite fonts in `text_skia.cpp`, the event loop in `application.cpp`
(`MsgWaitForMultipleObjectsEx` + a high-resolution waitable timer), and `uitk.manifest`
(per-monitor DPI v2, UTF-8 code page) linked into every executable.

**Compile check from Linux** (no Windows machine needed; catches compile errors, not link/runtime):
```bash
S=<scratchpad>
for f in src/core/*.cpp src/platform/windows/*.cpp src/render/skia/*.cpp src/render/cpu/*.cpp \
         src/render/gpu/backend_gpu.cpp src/render/gpu/wgl_context.cpp src/widgets/*.cpp \
         examples/*/*.cpp tools/*.cpp tests/*.cpp; do
  x86_64-w64-mingw32-g++ -std=c++20 -fsyntax-only -Wall -Wextra -Wpedantic -Wno-missing-field-initializers \
    -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0A00 -DSK_GANESH -DSK_GL -DSK_ENABLE_SVG -DSK_ENABLE_PARAGRAPH \
    -DSK_UNICODE_AVAILABLE -DSK_SHAPER_HARFBUZZ_AVAILABLE -DUITK_DEFAULT_ICON_DIR='"x"' -DUITK_HELLO_BACKEND='"CPU"' \
    -DUITK_ASSETS_DIR='"x"' -DUITK_HELLO_DIR='"x"' -Iinclude -Ibuild-linux/generated -isystem third_party/skia/src \
    "$f" 2>&1 | grep -E 'error|warning' && echo "^^ $f"
done
```
Public headers must survive `<windows.h>` and Xlib macros (`MOD_SHIFT`, `MOD_ALT`, `None`,
`Status`, `Bool`, `DrawText`, ...): that's why modifiers are `uitk::mod::Shift` etc.

**Real Windows build** happens in CI: `.github/workflows/windows.yml` (windows-2022, MSVC) builds
Skia (cached), all targets, runs ctest, renders snapshots, and smoke-tests that each example
opens a window and stays up. Results: `gh run list --workflow windows`, `gh run view <id> --log-failed`,
and `gh run download <id> -n windows-output -D <scratchpad>/win` for the snapshot PNGs and logs.

**On a Windows machine** (Developer PowerShell for VS 2022, with LLVM/clang-cl installed):
```powershell
powershell -ExecutionPolicy Bypass -File tools\build_skia.ps1 cpu    # once, ~20-40 min each
powershell -ExecutionPolicy Bypass -File tools\build_skia.ps1 gpu
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DUITK_RENDER_GPU=ON
cmake --build build-win
ctest --test-dir build-win --output-on-failure
build-win\workbench_gpu.exe
```
Skia is built with clang-cl, `/MD` and RTTI on; the app may use cl.exe or clang-cl. CMake forces
the `/MD` runtime in every configuration (Debug included), so the CRTs always match. `icudtl.dat`
is copied next to each executable (ICU loads it at startup). Logs: `hello: platform=win32
renderer=skia-ganesh-gl (<GPU>)`. `UITK_RENDERER=cpu` and `UITK_SCALE` work as on Linux.
