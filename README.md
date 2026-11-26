# fltk-dock

<img src="docs/assets/logo.svg" width="80" height="80" alt="fltk-dock logo">

[Homepage](https://hdijkema.github.io/fltk-dock/) |
[API reference and usage examples](docs/API.md)

Dockable tool windows for FLTK 1.4.5, written in C++17.

Panels can dock to any of the four host edges, float in independent native
windows, share an area with tabs, resize through splitters, and hide/reopen.
Dragging a title or dock tab moves the same content widget; it is not rebuilt.
The central document area expands into space released by tool panels.

## Build and try

Use CMake 3.21+, Git and a C++17 compiler. An installed FLTK 1.4.5 or newer
package is used when available; otherwise CMake fetches `release-1.4.5`.
For MSVC, run from an x64 Visual Studio developer command prompt:

```sh
cmake -S . -B build/dev -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/dev --parallel 4
ctest --test-dir build/dev --output-on-failure
build/dev/fltk-dock-demo.exe
```

Set `FLTK_DIR` explicitly to use an installed package. Its architecture and
runtime configuration must match the application. `FLTK_DOCK_FETCH_FLTK=OFF`
disables downloads. `FLTK_DOCK_BUILD_EXAMPLE` and `FLTK_DOCK_BUILD_TESTS` control
the demo and contract tests, defaulting to on for a standalone build.

## Use in an application

After `add_subdirectory` or `find_package(fltk-dock 0.1 CONFIG REQUIRED)`, link
`fltk-dock::fltk-dock` and include `<fltk_dock.h>`.

```cpp
Fl_Double_Window window(1000, 700, "Workspace");
auto *host = new fltk_dock::Host(0, 0, 1000, 700);
host->center(*new Fl_Box(0, 0, 400, 400, "Document"));
auto &browser = host->add_panel("browser", "Browser",
    *new Fl_Box(0, 0, 200, 300, "Objects"), fltk_dock::Side::Left, 240);
auto &queries = host->add_panel("queries", "Queries",
    *new Fl_Box(0, 0, 200, 300, "Saved queries"), fltk_dock::Side::Right, 240);
auto &history = host->add_panel("history", "History",
    *new Fl_Box(0, 0, 200, 300, "Previous queries"), fltk_dock::Side::Right, 240);
host->tabify(history, queries);
window.end();
window.resizable(host);
window.show();
```

Include `<FL/Fl_Box.H>` and `<FL/Fl_Double_Window.H>` for that example.
Like ordinary FLTK groups, `Host` takes ownership of heap-allocated content.
Use all library operations on the GUI thread. Do not independently delete
registered panels or their content. `Host` destroys floating and docked content
exactly once. Hide the owning window before destroying it.

## Docking interaction

- Drag a panel title or dock tab. Four blue arrow buttons mark the outer host
  edges; dropping there inserts outside panels already on that edge.
- Hover over a docked panel for local arrow buttons. Left/right arrows place a
  panel beside a left/right strip; top/bottom arrows place it beside a top/bottom
  strip. The center tab icon combines panels into a tabbed area. A blue outline
  previews the selected placement. Guides fade in and the outline eases between
  targets over 140 ms; releasing always applies the current target immediately.
  Drag an individual tab out to separate it.
- Placement order is saved with the layout, including panels inserted outside
  existing strips. Release away from a highlighted target to float the panel.
- Release over free space, or use the title's window button, to float a panel.
- Double-click the **inner panel title** to toggle floating/docked placement.
  The native floating-window caption moves the window normally; redock using
  its inner title, window button or context menu.
- Right-click a panel title for explicit dock/float/hide actions.
- Drag the separator between a docked area and its neighbor to resize it.
- The close control hides a tool window. Applications reopen it with
  `host->open(panel)`, usually from a View menu.

`save_layout()` returns `std::vector<PanelState>`; applications choose their
own serialization format. `restore_layout()` validates the snapshot before
changing the live layout, matches stable panel IDs, ignores unknown IDs, restores
tab groups/active tabs, and clamps floating windows to a current work area.
Register panels before restoring. Hidden/floating panels retain their content.
The host's visibility controls floating-window visibility as well.

## Reusable tabs

`<fltk_dock_tabs.h>` also supplies `fltk_dock::Tabs`, derived from `Fl_Tabs`.
It retains FLTK's selection, keyboard navigation and overflow menu, with
rectangular desktop styling. Its tab strip remains 30 pixels high when resized.
Dock `Panel` pages can be dragged out; ordinary document pages retain their
normal FLTK behavior. To enable a document-tab close button on the right,
set the page's `when(FL_WHEN_CLOSED)` and handle its callback. The application
decides whether to save, cancel a close or delete the page.

## Scope and validation

This first implementation uses ordered edge strips and tabbed tool areas around
one central document widget. It does not implement arbitrary recursive split
trees, auto-hide flyouts, moving a whole tab group as one unit, or docking across
different hosts. A single tab can move independently between edges and groups.
Results inside pgadmin-jr's document tabs remain part of their query document.

Windows/MSVC is the local validation target. Contract tests exercise ownership,
floating/redocking, hiding/reopening, tab groups, invalid snapshots and restore,
and title/tab drags, relative strip insertion and reordered layout restoration through
FLTK's event dispatcher. They render `docking.png` and `docking-guides.png`
into the build directory. These are programmatic GUI tests, not a completed
manual desktop interaction check. macOS/Linux and clean-machine distribution
have not been validated. The first source release tag is not published yet.

See [development notes](docs/DEVELOPMENT.md) and [LICENSE](LICENSE) (LGPL 2.1).
