---
name: build-and-run
description: Build the cpp-ui-toolkit Linux libraries and examples (CPU and GPU render backends, Wayland + X11) with CMake/Ninja, run tests, launch the hello example, and render offscreen snapshots to PNG for visual checks. Use when asked to build, run, launch, rebuild, test, benchmark, or screenshot the app, or to confirm a change works.
---

# Build and run cpp-ui-toolkit (Linux)

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
