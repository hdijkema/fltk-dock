# Development

`Host` owns one central widget and the registered `Panel` frames. Physical FLTK
parentage changes during docking: a panel is a child of the host, a shared `Tabs`
group, or its native floating window. Registration and logical ownership remain
with the host. Panel destruction first detaches itself from its floating window
before deleting that window, preventing recursive/double deletion.

`Host::layout` reserves edge strips in their current placement order, one strip per tab
group, and gives the remaining rectangle to the central widget. Splitter drags
change the registered extent of all panels sharing that area. A small central
area is retained as space permits. Closed panels and floating panels reserve
no docked space. Empty tab groups are hidden and reused.

Panel title drags and `Tabs` dock-tab drags use screen coordinates to preview a
host edge or another visible panel. Detachment happens on release, preserving
native mouse capture during the drag. Arrow buttons identify outer edges and
supported relative strip positions; the center tab icon explicitly requests
`tabify`. `drop_target` is shared by hit testing, preview and release, so the
highlighted action is the one applied. `dock_beside` changes strip order while
keeping a target tab group together. Outer drops insert before existing strips;
other drops float the panel. OS caption movement remains ordinary native window
movement. Snapshots preserve strip order through their vector order, so existing
application serializers need no additional fields.

`PanelState` contains placement, visibility, geometry and tab-group membership.
It contains no widget pointers or database/application data. `save_layout`
observes live floating geometry; `restore_layout` validates every entry first,
then restores existing IDs and tab selection. Applications own persistence.

`Tabs` subclasses `Fl_Tabs` for drawing, close-button hit testing and padding.
Selection, keyboard navigation, overflow and child lifetime remain FLTK's.
Only pages whose actual type is `Panel` gain dock-tab dragging. Ordinary document
pages can use the same appearance without becoming dock windows.

Build and test on Windows before publishing a version tag. Use matching MSVC
runtime/build configurations for FLTK and consumers. `tests/consumer` checks the
installed CMake package. No tests access a user's files, settings or databases.
Native desktop interaction and other platforms still require validation.

Dock feedback uses a 140 ms cubic ease-out with steady-clock timestamps. A short
FLTK timer repaints only while the guides or outline are changing; it does not
run continuously or delay hit testing/release. Moving to another target restarts
from the currently displayed rectangle. Hiding or destroying the Host and
finishing a drag remove the pending callback. No GUI thread sleeps are used.

## API documentation

`docs/API.md` documents this repository's public headers, ownership rules and
CMake integration, with a complete C++ example. Update it when the public API
changes. The Penthario website links here rather than maintaining a second copy.

## Library website

`docs/index.html` is the standalone landing page. `docs/assets/logo.svg` is the
library's vector logo and favicon; `docs/style.css` provides responsive layout
and explicit light/dark themes. All assets and fonts are local or system-provided;
the only script is the small theme control; the library itself has no website dependency.

GitHub Pages can publish the default branch's `/docs` directory. Jekyll uses
`docs/_config.yml`, `jekyll-optional-front-matter` and the documentation layout
to render `API.md` and this guide as HTML. Repository-relative source links in
the Markdown are mapped to GitHub by the layout. Documentation stays owned by
this repository. The landing illustration is an HTML/CSS diagram, not a product
screenshot. The logo uses the same rounded-square geometry as the other widget
libraries, with a distinct symbol and accent color.

To build locally with Jekyll 3.10 and `kramdown-parser-gfm` installed:

```sh
jekyll build --source docs --destination build/site/fltk-dock
python -m http.server 8000 --directory build/site
```

Open `http://localhost:8000/fltk-dock/`. The destination includes the configured
repository base path so API-page assets resolve correctly. Publishing requires the repository's GitHub Pages setting;
adding these source files does not change that setting or publish the site.

## Website theme control

The homepage and documentation layout start with `data-theme="dark"`.
`theme.js` restores a saved light/dark choice before the body paints and attaches
the keyboard-accessible header button. The preference uses the site's own
`data-theme-key` in local storage; storage failures do not prevent switching.
The default is dark even when the operating system prefers light. Without
JavaScript, the pages retain their default dark appearance.
