# UI Controls

Inventory of the controls in **uitk**: what exists, where it lives, and what is still planned.
Mark a row ✅ once the control is designed, implemented, tested and themed.

**Legend:** ✅ done · 🟡 done with known gaps (see notes) · ⬜ planned
**Demo:** shown in the `hello` gallery (`examples/hello/demo.cpp`)

Every control is built from the shared layers rather than its own code:
`Theme` (design tokens: VS Code Dark/Light, Adwaita Dark/Light), `StateLayer` (animated
hover/press/focus/disabled/checked), `Signal<…>` (events), `glyph()` / `icons::symbolic()`
(icons), `WidgetHost` (input routing, focus, popups, tooltips) and `Box` (layout).

---

## Basic controls

| Status | Control | Class | Header | Demo | Notes |
|:--:|---|---|---|:--:|---|
| ✅ | Label | `Label` | `widgets/label.h` | ✓ | Text roles (body, dim, caption, chrome, heading), wrap, ellipsis, colour tokens |
| ✅ | Button | `Button` | `widgets/button.h` | ✓ | Primary, Secondary, Destructive, Flat; label, icon or both; Space/Enter |
| ✅ | Icon button | `Button(Image, Flat/Circular)` | `widgets/button.h` | ✓ | Toolbar-style square or round |
| ✅ | Toggle button | `ToggleButton` | `widgets/toggles.h` | ✓ | Stays on while checked (accent outline); text or icon |
| ✅ | Link button | `Button(…, ButtonStyle::Link)` | `widgets/button.h` | ✓ | Accent text, underline fades in on hover, hand cursor |
| ✅ | Check box | `CheckBox` | `widgets/toggles.h` | ✓ | Animated check, indeterminate state, label click target |
| ✅ | Radio button | `RadioButton` + `RadioGroup` | `widgets/toggles.h` | ✓ | Exclusive selection, dot grows in; demo radios switch the theme |
| ✅ | Switch | `Switch` | `widgets/toggles.h` | ✓ | Knob slides with the checked animation |
| 🟡 | Text field | `TextField` | `widgets/text_field.h` | ✓ | Caret, selection (mouse, Shift, double/triple click), word jumps/deletes, placeholder, UTF-8. **Gaps:** clipboard (needs platform clipboard), undo/redo, IME pre-edit, password mode |
| ✅ | Spin button | `SpinButton` | `widgets/text_field.h` | ✓ | Typed or ± / Up/Down/PageUp/PageDown; clamps; invalid input restored |
| ✅ | Dropdown (select) | `Dropdown` + `MenuList` | `widgets/dropdown.h` | ✓ | Popup list, keyboard (Space/Enter/Alt+Down, arrows, Escape), check on current item |
| ✅ | Slider | `Slider` | `widgets/range.h` | ✓ | Drag, click-to-jump, step snapping, arrows/PageUp/Home/End, hover halo |
| ✅ | Progress bar | `ProgressBar` | `widgets/range.h` | ✓ | Animated value; indeterminate sweep (animates only while visible) |
| ✅ | Spinner | `Spinner` | `widgets/range.h` | ✓ | Rotating arc; animates only while running and visible |
| ✅ | Badge | `Badge` | `widgets/containers.h` | ✓ | Count / status pill |
| ✅ | Tooltip | `Widget::set_tooltip()` | `widgets/widget.h` | ✓ | Shown by the host after 600 ms hover; hidden on press/key/leave |
| ✅ | Icon | `Icon` | `widgets/icon.h` | ✓ | Tinted symbolic image; follows theme colour tokens |
| ✅ | Separator | `Separator` | `widgets/frame.h` | ✓ | Horizontal or vertical hairline |
| ✅ | Frame / card | `Frame` | `widgets/frame.h` | – | Card, input and panel surfaces |
| ✅ | Tabs | `TabView` | `widgets/containers.h` | ✓ | Any widget per page, sliding indicator, Left/Right on the tab strip |
| ✅ | Scroll view | `ScrollView` + `ScrollBar` | `widgets/containers.h` | ✓ | Spring-smoothed wheel, overlay scrollbar (drag / page), PageUp/Down, keyboard focus scrolled into view. Vertical only for now |
| ✅ | Box layout | `Box`, `Spacer` | `widgets/box.h` | ✓ | Rows/columns, spacing, padding, alignment, expand/shrink, height-for-width |

## Window chrome

| Status | Control | Class | Header | Demo | Notes |
|:--:|---|---|---|:--:|---|
| ✅ | Application window | `ApplicationWindow` | `widgets/application_window.h` | ✓ | Window + renderer + shell; close quits unless handled |
| ✅ | Window frame | `WindowFrame` | `widgets/window_frame.h` | ✓ | Client-side shadow, rounded corners, resize edges |
| ✅ | Header bar | `HeaderBar` | `widgets/header_bar.h` | ✓ | start/end slots, plain title or command-center box, theme-driven window controls |
| ✅ | Status bar | `StatusBar` | `widgets/status_bar.h` | ✓ | Clickable items, left and right groups |

## Complex controls

Not part of the basic gallery. Some already exist in the library.

| Status | Control | Class | Header | Notes |
|:--:|---|---|---|---|
| ✅ | Split button | `SplitButton` | `widgets/split_button.h` | Action + chevron (`menu_requested`) |
| ✅ | List box / list row | `ListBox`, `ListRow` | `widgets/list.h` | Selection, hover actions, arrow keys. Use `ListView` for long lists |
| ✅ | Notification (toast) | `Notification` | `widgets/notification.h` | Message, source, actions, settings, close |
| ✅ | Virtualized list | `ListView`, `ListModel` | `widgets/list_view.h` | Model-driven; paints only visible rows. Single/multiple selection (Ctrl, Shift), keyboard, type-ahead, placeholder for empty lists |
| ✅ | File browser | `FileBrowser` | `widgets/file_browser.h` | In-app (no native dialog): history, editable path, places, sortable Name/Size/Modified, filter + type filters, hidden files, open-file/files/folder modes |
| ✅ | File dialog | `FileDialog` | `widgets/file_browser.h` | Modal, centred `FileBrowser` with Cancel / Open (`FileDialog::open(host, …)`) |
| ✅ | File upload | `FileUpload` | `widgets/file_upload.h` | Dashed drop zone + Browse, type/size/count limits with messages, per-file progress and Pending/Uploading/Done/Failed state. **Gap:** OS drag and drop |
| ⬜ | Menu bar | | | File/Edit/… with keyboard navigation (build on `MenuList` + popup layer) |
| ⬜ | Context menu / menu | | | Nested submenus, shortcuts, separators, check/radio items |
| 🟡 | Dialog / modal | `PopupOptions::modal` | `widgets/widget_host.h` | Modal layer, scrim, focus trap, Escape. **Gap:** a generic `Dialog` widget (FileDialog is the first user) |
| ⬜ | Tree view | | | Expand/collapse, lazy children, virtualized |
| ⬜ | Table / data grid | | | Columns, sorting, resizing, virtualized rows |
| ⬜ | Multi-line text area | | | Wrapping editor; foundation for the code editor (M4) |
| ⬜ | Code editor | | | Piece-table buffer, syntax highlighting, gutter, minimap |
| ⬜ | Search / filter field | | | Text field with icon, clear button, history |
| ⬜ | Splitter / paned | | | Draggable divider between panes |
| ⬜ | Accordion / expander | | | Collapsible sections |
| ⬜ | Toolbar | | | Overflow menu for buttons that do not fit |
| ⬜ | Breadcrumbs | | | Path segments with dropdowns |
| ⬜ | Command palette | | | Fuzzy search over commands (VS Code Ctrl+Shift+P) |
| ⬜ | Color picker | | | |
| ⬜ | Date / time picker | | | |

## Shared infrastructure

| Status | Piece | Where | Notes |
|:--:|---|---|---|
| ✅ | Theme presets + derived tokens | `widgets/theme.h` | VS Code Dark/Light, Adwaita Dark/Light; `Theme::derive()` fills component colours |
| ✅ | Interaction states | `widgets/state.h` (`StateLayer`) | Hover/press/focus-visible/disabled/checked/hover-within, animated |
| ✅ | Popup layer | `WidgetHost::show_popup()` | Positioning (flip, clamp), outside-click and Escape dismissal, focus return, shadow |
| ✅ | Tooltips | `WidgetHost` | Hover delay, single bubble |
| ✅ | Keyboard focus | `WidgetHost` | Click focus, Tab/Shift+Tab, focus ring only for keyboard focus, `scroll_into_view()` |
| ✅ | Text editing support | `TextLayout::caret_x/index_at/range_rects` | UTF-8 ↔ UTF-16 mapping for Skia paragraphs |
| ✅ | Animation | `core/animation.h` | Easing (CSS bezier), transitions, springs, loops; frames only while animating |
| ✅ | Modal popups | `PopupOptions` | Modal + centred popups, scrim, input blocked below, Tab scoped to the modal |
| ✅ | Helpers | `core/format.h`, `icons::for_file`, `Paint::dashed` | Byte sizes / dates, file-type icons, dashed strokes |
| ✅ | Platform: Linux | `src/platform/linux` | Wayland + X11, picked at runtime; client-side shadow |
| 🟡 | Platform: Windows | `src/platform/windows` | Win32, DWM shadow + rounded corners, snap, per-monitor DPI v2, UTF-8 text, WGL GPU backend, DirectWrite fonts. Compile-checked from Linux (mingw) and built/tested by CI (`.github/workflows/windows.yml`); not yet tried by hand. **Gaps:** clipboard, IME candidate window placement, Windows 11 snap-layout flyout on the maximize button |
| ⬜ | Platform: macOS | | |
| ⬜ | Clipboard | platform | Wayland `wl_data_device`, X11 selections, Win32 clipboard; enables copy/paste in text fields |
| ⬜ | IME / pre-edit | platform | `zwp_text_input_v3`, XIM pre-edit, IMM32/TSF composition |
| ⬜ | Accessibility | platform | AT-SPI tree |
| ⬜ | Drag and drop | platform | |
| ⬜ | Partial redraw | render | Repaint only damaged regions |

## Tests

Behaviour is covered by `tests/test_widgets.cpp` and `tests/test_controls.cpp`. They drive real
pointer/keyboard events through an offscreen `WindowShell`; run them with
`ctest --test-dir build-linux`. To see a control, render the gallery offscreen with
`uitk_snapshot_cpu`. Options include `--dark`, `--style adwaita`, `--scroll Y`, `--dropdown`,
`--tooltip`, `--type TEXT`, `--hover …` and `--focus`.
