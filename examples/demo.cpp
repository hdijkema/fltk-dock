#include <fltk_dock.h>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>

int main()
{
    Fl_Double_Window window(1100, 740, "FLTK docking windows");
    auto *host = new fltk_dock::Host(0, 35, 1100, 705);
    auto *center = new Fl_Box(0, 0, 300, 300, "Central document area");
    center->box(FL_FLAT_BOX);
    center->color(FL_WHITE);
    host->center(*center);
    auto &browser = host->add_panel("browser", "Database browser",
        *new Fl_Box(0, 0, 200, 300, "Drag this panel's title bar"), fltk_dock::Side::Left, 250);
    auto &queries = host->add_panel("queries", "Saved queries",
        *new Fl_Box(0, 0, 200, 300, "Drop onto another panel to form tabs"), fltk_dock::Side::Right, 250);
    auto &history = host->add_panel("history", "History",
        *new Fl_Box(0, 0, 200, 300, "Drag a tab's title bar to move its panel"), fltk_dock::Side::Bottom, 200);
    host->tabify(history, queries);
    auto *reset = new Fl_Button(8, 4, 140, 27, "Reset layout");
    struct Defaults { fltk_dock::Host *host; std::vector<fltk_dock::PanelState> layout; } defaults{host, host->save_layout()};
    reset->callback([](Fl_Widget *, void *p)
    {
        auto &defaults = *static_cast<Defaults *>(p);
        defaults.host->restore_layout(defaults.layout);
    }, &defaults);
    window.end();
    window.resizable(host);
    window.size_range(600, 400);
    window.show();
    return Fl::run();
}
