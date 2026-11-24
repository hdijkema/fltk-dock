#pragma once
#include <FL/Fl_Tabs.H>

namespace fltk_dock
{
class Panel;
// Rectangular desktop tabs using FLTK's selection, keyboard and overflow behavior.
// Children remain owned by Fl_Tabs. Set FL_WHEN_CLOSED and a child callback to handle close requests.
// draw_tab paints flat borders and a right-side close glyph; hit_close uses the same coordinates.
class Tabs : public Fl_Tabs
{
  public:
    Tabs(int, int, int, int, const char *label = nullptr);
    // Keep a 30-pixel tab strip and fit each fully constructed child to the client area.
    void resize(int, int, int, int) override;
    // Dock pages can be dragged by their tabs; ordinary application tabs retain FLTK event handling.
    int handle(int) override;

  protected:
    void draw_tab(int, int, int, int, Fl_Widget *, int, int) override;
    int hit_close(Fl_Widget *, int, int) override;
    int tab_positions() override;
  private:
    Panel *dragged_ = nullptr;
    int press_x_ = 0, press_y_ = 0;
    bool moved_ = false;
};
} // namespace fltk_dock
