#include <fltk_dock.h>
#include <FL/Fl_Box.H>

int main()
{
    fltk_dock::Host host(0, 0, 800, 600);
    host.center(*new Fl_Box(0, 0, 100, 100));
    auto &pane = host.add_panel("test", "Test", *new Fl_Box(0, 0, 100, 100), fltk_dock::Side::Left);
    host.open(pane, false);
    host.open(pane);
    return host.save_layout().size() == 1 && pane.is_open() ? 0 : 1;
}
