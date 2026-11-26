# fltk-dock API and examples

[README](../README.md) · [Source](https://github.com/hdijkema/fltk-dock)

The library owns dockable tool panels around one central widget. Panels can
share tabs, float in native windows and hide without losing their content.
Namespace: `fltk_dock`. All operations require the FLTK GUI thread.

## Headers and linking

`<fltk_dock.h>` declares the host, panels and snapshots, and includes
`<fltk_dock_tabs.h>`. Include the latter directly for standalone document tabs.

```cmake
cmake_minimum_required(VERSION 3.21)
project(dock-example LANGUAGES CXX)
find_package(fltk-dock 0.1 CONFIG REQUIRED)
add_executable(example main.cpp)
target_compile_features(example PRIVATE cxx_std_17)
target_link_libraries(example PRIVATE fltk-dock::fltk-dock)
```

## Side and PanelState

Declared in [fltk_dock.h](../include/fltk_dock.h).

`enum class Side { Left, Right, Top, Bottom };` identifies an edge.
`PanelState` is an application-owned value with these public fields:

| Field | Default | Meaning |
| --- | --- | --- |
| `std::string id` | empty | Stable registered panel ID. |
| `Side side` | `Side::Left` | Dock edge, also remembered when floating. |
| `int extent` | `240` | Width of a left/right strip or height of a top/bottom strip. |
| `bool open` | `true` | Whether the panel is open. |
| `bool floating` | `false` | Whether it occupies a floating window. |
| `int x`, `int y` | `100`, `100` | Floating window screen coordinates. |
| `int w`, `int h` | `400`, `400` | Floating window dimensions. |
| `std::string tab_group` | empty | Shared docking tab group identifier, if any. |
| `bool active` | `false` | Active panel within a tab group. |

Keep the vector order returned by `save_layout()`: it also records strip
placement order. Store all fields when serializing a snapshot.

## Host

`Host` derives from `Fl_Group`. Its public API is:

| Declaration | Preconditions and result |
| --- | --- |
| `Host(int x, int y, int w, int h)` | Create a host group. |
| `~Host() override` | Destroy owned panels, floating windows and content. Hide the owning window before destruction. |
| `void center(Fl_Widget& content)` | Adopt one heap widget as the center; set it once before panel interaction. Replacing an existing center throws `std::invalid_argument`. |
| `Panel& add_panel(std::string id, const char* title, Fl_Widget& content, Side side, int extent = 240)` | Adopt a heap content widget and return its host-owned frame. IDs must be nonempty and unique, content unique, side valid and extent 80–4096; invalid registration throws `std::invalid_argument`. |
| `void dock(Panel& panel, Side side)` | Dock a panel owned by this host at the supplied edge. |
| `void dock_beside(Panel& panel, Panel& target, Side position)` | Insert beside a visible strip in the same host. Left/right strips accept Left/Right; top/bottom strips accept Top/Bottom. Target tab groups remain together. |
| `void tabify(Panel& panel, Panel& target)` | Place the panel in the target's tabbed docking area. Both panels must belong to this host. |
| `void float_panel(Panel& panel, int screen_x, int screen_y)` | Float an owned panel without rebuilding content. |
| `void open(Panel& panel, bool open = true)` | Reopen or hide a registered panel, retaining its placement and content. |
| `std::vector<PanelState> save_layout() const` | Return an application-owned snapshot of registered panels. No files are written. |
| `void restore_layout(const std::vector<PanelState>& states)` | Register panels first. Validate the full snapshot before changing the layout, match IDs, ignore unknown IDs and clamp floating geometry to a screen. Invalid snapshots throw `std::invalid_argument`. |
| `void resize(int x, int y, int w, int h) override` | Resize and relayout the host. |
| `void layout()` | Lay out ordered edge strips, tabs and splitters, then give the remaining rectangle to the center widget. |
| `int handle(int event) override` | Handle splitter, docking and preview interaction through the FLTK event loop. |
| `void draw() override` (protected) | Paint the host and docking feedback. |

Operations taking panels reject ownership mismatches. Invalid docking directions
or incompatible relative placement also throw `std::invalid_argument`.
Layout persistence is separate from content persistence: save editor text and
application data yourself.

Snapshot validation requires nonempty, unique IDs, a valid side, extent 80–4096,
width/height 1–32768, a tab-group name of at most 256 bytes, and x/y coordinates
within ±1,000,000. Unknown panel IDs are ignored only after the whole input has
passed validation. Panels absent from the snapshot retain their state.

## Panel

`Panel` derives from `Fl_Group`. Construction is private; use `Host::add_panel`.
The host owns each frame. Do not delete a panel or its registered content yourself.

| Declaration | Result |
| --- | --- |
| `~Panel() override` | Release the frame's owned resources; destruction normally belongs to the host. |
| `const std::string& id() const` | Borrow the stable ID. |
| `bool is_open() const` | Return the saved open state. |
| `bool is_floating() const` | Return floating placement state. |
| `Side side() const` | Return the remembered dock edge. |
| `Fl_Widget& content() const` | Borrow the owned content widget, including from a const panel. |
| `void resize(int x, int y, int w, int h) override` | Resize the frame and its content. |
| `int handle(int event) override` | Dispatch title drag, double-click and context-menu interaction. |
| `void draw() override` (protected) | Paint the panel frame and title. |

Drag a panel title or dock tab toward the host's arrow guides. The center guide
creates a tab group. Releasing away from a guide floats the panel. The close
control hides it; reopen with `host.open(panel)`. Double-click the inner title
to toggle floating/docked placement. The native window caption moves a floating
window normally.

## Tabs

Declared in [fltk_dock_tabs.h](../include/fltk_dock_tabs.h).
`Tabs` derives from `Fl_Tabs` and retains native selection, keyboard navigation,
child ownership and overflow behavior.

| Declaration | Result |
| --- | --- |
| `Tabs(int x, int y, int w, int h, const char* label = nullptr)` | Create rectangular document or docking tabs. |
| `void resize(int x, int y, int w, int h) override` | Keep a 30-pixel strip and fit constructed pages to the client area. |
| `int handle(int event) override` | Permit docking-panel tab drags; ordinary pages use native FLTK handling. |
| `void draw_tab(int, int, int, int, Fl_Widget*, int, int) override` (protected) | Paint a tab and its optional close glyph. |
| `int hit_close(Fl_Widget*, int, int) override` (protected) | Test the same close-glyph geometry used for drawing. |
| `int tab_positions() override` (protected) | Calculate tab placement for the custom strip. |

For an ordinary page, enable a close request with
`page->when(FL_WHEN_CLOSED)` and `page->callback(...)`. The callback decides
whether to save, cancel or remove the page; the library does not make that
decision for the application.

## Complete example: two tabbed tool panels

Save as `main.cpp`. The window owns the host; the host adopts the center and
panel content. A saved snapshot is restored after a temporary placement change.

```cpp
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <fltk_dock.h>

int main()
{
    Fl_Double_Window window(960, 640, "Docking example");
    auto* host = new fltk_dock::Host(0, 0, 960, 640);
    host->center(*new Fl_Box(0, 0, 400, 400, "Document area"));
    auto& browser = host->add_panel("browser", "Browser",
        *new Fl_Box(0, 0, 200, 300, "Objects"),
        fltk_dock::Side::Left, 220);
    auto& history = host->add_panel("history", "History",
        *new Fl_Box(0, 0, 200, 300, "Recent commands"),
        fltk_dock::Side::Left, 220);
    host->tabify(history, browser);
    host->end();
    window.end();
    window.resizable(host);

    const auto layout = host->save_layout();
    host->dock(history, fltk_dock::Side::Bottom);
    host->restore_layout(layout);
    window.show();
    const int result = Fl::run();
    window.hide();
    return result;
}
```

## Standalone document tabs

Inside an open window group, create pages as ordinary FLTK children:

```cpp
auto* tabs = new fltk_dock::Tabs(10, 10, 700, 450);
auto* first = new Fl_Group(10, 40, 700, 420, "First query");
new Fl_Box(20, 50, 250, 80, "First document");
first->end();
auto* second = new Fl_Group(10, 40, 700, 420, "Second query");
new Fl_Box(20, 50, 250, 80, "Second document");
second->end();
tabs->end();
tabs->value(first);
```

## Scope and build options

The host uses ordered edge strips with tabbed areas around one center. It does
not implement arbitrary recursive split trees, auto-hide flyouts, whole-group
dragging or docking across hosts.
`FLTK_DOCK_BUILD_EXAMPLE` and `FLTK_DOCK_BUILD_TESTS` default to ON standalone.
`FLTK_DOCK_FETCH_FLTK=OFF` requires an installed FLTK package; otherwise a
missing FLTK installation can be fetched at `release-1.4.5`.

## Building the example

Save the complete C++ program as `main.cpp` and the CMake block as
`CMakeLists.txt` in an empty example directory. Set `CMAKE_PREFIX_PATH` to your
installed library and FLTK prefixes, for example:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/sdk/widgets;C:/sdk/fltk"
cmake --build build --config Release
```

Use your actual installation paths and a Visual Studio developer terminal on
Windows. Match architecture, runtime and Release/Debug configuration. See the
[README](../README.md) for library build and installation instructions.

This reference describes the public headers in this repository. It covers the
library's declared API; inherited FLTK methods and private implementation helpers
are outside its scope. Keep it aligned with header changes. Windows is the local
validation target; this reference does not establish macOS/Linux runtime or
clean-machine distribution validation.
