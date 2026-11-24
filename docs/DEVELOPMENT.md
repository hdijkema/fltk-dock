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
