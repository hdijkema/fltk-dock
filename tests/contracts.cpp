#include <fltk_dock.h>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_PNG_Image.H>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
struct Content : Fl_Box
{
    int &destroyed;
    Content(int &counter) : Fl_Box(0, 0, 200, 200, "Preserved content"), destroyed(counter) {}
    ~Content() override
    {
        ++destroyed;
    }
};

// Exercise our own event handlers through FLTK's dispatcher; no operating-system input is injected.
// FLTK 1.4 exposes these event fields for its platform drivers; this test process owns all of them.
void mouse(Fl_Window &window, int event, int x, int y)
{
    Fl::e_x = x;
    Fl::e_y = y;
    Fl::e_x_root = window.x() + x;
    Fl::e_y_root = window.y() + y;
    Fl::e_keysym = FL_Button + FL_LEFT_MOUSE;
    Fl::e_state = event == FL_RELEASE ? 0 : FL_BUTTON1;
    Fl::e_clicks = 0;
    Fl::e_is_click = event == FL_PUSH;
    Fl::handle(event, &window);
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        Fl::lock();
        int destroyed = 0;
        {
            Fl_Double_Window window(1000, 700, "Docking contracts");
            auto *host = new fltk_dock::Host(0, 0, 1000, 700);
            auto *center = new Content(destroyed);
            host->center(*center);
            auto &a = host->add_panel("a", "Browser", *new Content(destroyed), fltk_dock::Side::Left, 220);
            auto &b = host->add_panel("b", "Queries", *new Content(destroyed), fltk_dock::Side::Right, 240);
            auto &c = host->add_panel("c", "History", *new Content(destroyed), fltk_dock::Side::Bottom, 180);
            window.end();
            window.resizable(host);
            require(center->w() == 530 && center->h() == 515, "Center receives remaining edge layout");
            host->tabify(c, b);
            require(b.parent() == c.parent() && dynamic_cast<fltk_dock::Tabs *>(b.parent()),
                    "Panels share FLTK tabs");
            require(center->h() == 700, "Tabbed panel no longer reserves a second strip");
            host->open(c, false);
            require(!c.is_open() && b.parent() != c.parent(),
                    "Hidden dock tab is removed from the tab strip");
            host->open(c);
            require(b.parent() == c.parent(), "Reopening returns a panel to its tab group");
            auto saved = host->save_layout();
            auto *content = &c.content();
            host->float_panel(c, 200, 150);
            require(c.is_floating() && c.window() != &window && &c.content() == content,
                    "Floating preserves content");
            host->dock(c, fltk_dock::Side::Top);
            require(!c.is_floating() && c.side() == fltk_dock::Side::Top && &c.content() == content,
                    "Redocking preserves content");
            host->restore_layout(saved);
            require(b.parent() == c.parent() && !c.is_floating(), "Restore recreates tabbed docking");
            auto invalid = saved;
            invalid[0].open = false;
            invalid[1].extent = -1;
            bool rejected = false;
            try
            {
                host->restore_layout(invalid);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            require(rejected && a.is_open(), "Invalid restore is rejected before changing any panel");
            host->dock(a, fltk_dock::Side::Bottom);
            host->resize(0, 0, 700, 500);
            require(center->w() >= 180 && center->h() >= 120, "Resizing retains a usable central area");
            host->restore_layout(saved);
            host->resize(0, 0, 1000, 700);
            window.show();
            Fl::check();
            mouse(window, FL_PUSH, a.x() + 50, a.y() + 14);
            mouse(window, FL_DRAG, host->w() / 2, host->h() - 4);
            mouse(window, FL_RELEASE, host->w() / 2, host->h() - 4);
            require(a.side() == fltk_dock::Side::Bottom && !a.is_floating(),
                    "Title drag docks to a host edge");
            host->restore_layout(saved);
            Fl::check();
            auto *group = dynamic_cast<fltk_dock::Tabs *>(c.parent());
            require(group && group->children() == 2, "Both dock tabs remain registered");
            group->value(&c);
            Fl::check();
            const int tab_x = group->x() + group->w() / 2;
            mouse(window, FL_PUSH, tab_x, group->y() + 14);
            mouse(window, FL_DRAG, host->w() / 2, host->h() - 4);
            mouse(window, FL_RELEASE, host->w() / 2, host->h() - 4);
            require(c.parent() != b.parent() && c.side() == fltk_dock::Side::Bottom,
                    "Dragging a dock tab moves that panel out of its group");
            host->restore_layout(saved);
            Fl::check();
            auto *right_tabs = static_cast<fltk_dock::Tabs *>(b.parent());
            right_tabs->value(&b);
            Fl::check();
            const int arrow_x = right_tabs->x() + right_tabs->w() / 2 + 34;
            const int arrow_y = right_tabs->y() + right_tabs->h() / 2;
            mouse(window, FL_PUSH, a.x() + 50, a.y() + 14);
            mouse(window, FL_DRAG, arrow_x, arrow_y);
            if (argc > 1)
            {
                auto directory = std::filesystem::u8path(argv[1]);
                std::filesystem::create_directories(directory);
                // Let feedback settle through the real event loop before recording the guide artifact.
                const auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(180);
                while (std::chrono::steady_clock::now() < settled)
                    Fl::wait(0.01);
                Fl_Image_Surface guides(window.w(), window.h());
                Fl_Surface_Device::push_current(&guides);
                guides.draw(&window);
                std::unique_ptr<Fl_RGB_Image> image(guides.image());
                Fl_Surface_Device::pop_current();
                require(fl_write_png((directory / "docking-guides.png").u8string().c_str(), image.get()) == 0,
                        "Docking arrows render artifact");
            }
            mouse(window, FL_RELEASE, arrow_x, arrow_y);
            require(a.side() == fltk_dock::Side::Right && a.x() > b.x() && b.parent() == c.parent(),
                    "Right arrow inserts outside an existing right tab group");
            auto reordered = host->save_layout();
            host->dock_beside(a, b, fltk_dock::Side::Left);
            require(a.x() < b.x(), "Left insertion puts a panel inside the right strip");
            host->restore_layout(reordered);
            require(a.x() > b.x(), "Snapshot restores reordered strips, not registration order");
            host->dock(c, fltk_dock::Side::Bottom);
            mouse(window, FL_PUSH, c.x() + 50, c.y() + 14);
            mouse(window, FL_DRAG, host->w() - 4, host->h() / 2);
            mouse(window, FL_RELEASE, host->w() - 4, host->h() / 2);
            require(c.x() > a.x() && c.x() > b.x(), "Outer edge inserts beyond all existing right strips");
            host->dock_beside(c, a, fltk_dock::Side::Left);
            require(c.x() < a.x(), "Individual panel can move to either side of another strip");
            host->dock(a, fltk_dock::Side::Bottom);
            host->dock_beside(c, a, fltk_dock::Side::Bottom);
            require(c.y() > a.y(), "Bottom insertion is ordered outside the bottom strip");
            host->dock_beside(c, a, fltk_dock::Side::Top);
            require(c.y() < a.y(), "Top insertion is ordered inside the bottom strip");
            host->restore_layout(saved);
            Fl::check();
            Fl_Image_Surface surface(window.w(), window.h());
            Fl_Surface_Device::push_current(&surface);
            surface.draw(&window);
            std::unique_ptr<Fl_RGB_Image> image(surface.image());
            Fl_Surface_Device::pop_current();
            if (argc > 1)
            {
                auto directory = std::filesystem::u8path(argv[1]);
                std::filesystem::create_directories(directory);
                require(fl_write_png((directory / "docking.png").u8string().c_str(), image.get()) == 0,
                        "Docking layout render artifact");
            }
            for (int i = 0; i < 5; ++i)
            {
                host->float_panel(c, 200 + i * 10, 150);
                host->open(c, false);
                host->open(c);
                host->tabify(c, b);
            }
            host->float_panel(c, 200, 150);
            // Close with feedback still animating; queued callbacks must not outlive their Host.
            mouse(window, FL_PUSH, a.x() + 50, a.y() + 14);
            mouse(window, FL_DRAG, host->w() - 4, host->h() / 2);
            window.hide();
            require(!c.window()->shown(), "Owner hide also hides floating windows");
        }
        const auto drained = std::chrono::steady_clock::now() + std::chrono::milliseconds(180);
        while (std::chrono::steady_clock::now() < drained)
            Fl::wait(0.01);
        require(destroyed == 4, "Host destroys docked and floating content exactly once");
        std::cout << "Docking contracts passed\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
