#include "fltk_dock_tabs.h"
#include "fltk_dock.h"
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>

namespace fltk_dock
{
Tabs::Tabs(int x, int y, int w, int h, const char *label) : Fl_Tabs(x, y, w, h, label)
{
    box(FL_FLAT_BOX);
    color(fl_rgb_color(240, 240, 240));
    selection_color(FL_WHITE);
    labelsize(14);
    handle_overflow(OVERFLOW_PULLDOWN);
}

// Reserve desktop-style horizontal padding without changing FLTK's overflow/selection algorithm.
int Tabs::tab_positions()
{
    const int selected = Fl_Tabs::tab_positions();
    for (int i = 0; i < children(); ++i)
    {
        tab_pos[i] += i * 18;
        tab_width[i] += 18;
    }
    if (children())
        tab_pos[children()] += children() * 18;
    return selected;
}

void Tabs::resize(int x, int y, int w, int h)
{
    Fl_Widget::resize(x, y, w, h);
    for (int i = 0; i < children(); ++i)
        child(i)->resize(x + 1, y + 30, std::max(1, w - 2), std::max(1, h - 31));
    init_sizes();
    tab_offset = 0;
    if (auto *selected = value())
        Fl_Tabs::value(selected);
    redraw();
}

// Reuse Fl_Tabs to select pages; intercept only completed drags of dock Panel pages.
int Tabs::handle(int event)
{
    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE && Fl::event_y() < y() + 30)
    {
        auto *page = which(Fl::event_x(), Fl::event_y());
        dragged_ = page && !(page->when() & FL_WHEN_CLOSED) ? dynamic_cast<Panel *>(page) : nullptr;
        press_x_ = Fl::event_x_root();
        press_y_ = Fl::event_y_root();
        moved_ = false;
    }
    if (event == FL_DRAG && dragged_)
    {
        moved_ =
            moved_ || std::abs(Fl::event_x_root() - press_x_) + std::abs(Fl::event_y_root() - press_y_) > 6;
        if (moved_)
        {
            dragged_->host_.preview(Fl::event_x_root(), Fl::event_y_root(), dragged_);
            return 1;
        }
    }
    if (event == FL_RELEASE && dragged_)
    {
        auto *panel = dragged_;
        dragged_ = nullptr;
        if (moved_)
        {
            moved_ = false;
            push(nullptr);
            auto &host = panel->host_;
            host.finish_drag(*panel, Fl::event_x_root(), Fl::event_y_root(), 50, 14);
            return 1;
        }
    }
    return Fl_Tabs::handle(event);
}

// Draw each tab inside FLTK's tab clipping region; no platform theme or widget ownership is changed.
void Tabs::draw_tab(int x1, int, int width, int height, Fl_Widget *page, int flags, int)
{
    x1 += tab_offset;
    const bool selected = page == value();
    const int top = height >= 0 ? y() : y() + h() + height;
    height = std::abs(height);
    const Fl_Color background = selected ? FL_WHITE : fl_rgb_color(235, 235, 235);
    fl_color(background);
    fl_rectf(x1, top + 1, width, height);
    fl_color(fl_rgb_color(180, 180, 180));
    fl_rect(x1, top + 1, width, height);
    if (selected)
    {
        fl_color(fl_rgb_color(0, 120, 215));
        fl_rectf(x1 + 1, top + 1, width - 2, 2);
        fl_color(background);
        fl_line(x1 + 1, top + height, x1 + width - 2, top + height);
    }
    const bool close = (page->when() & FL_WHEN_CLOSED) && !(flags & 1);
    fl_font(page->labelfont(), page->labelsize());
    fl_color(fl_rgb_color(30, 30, 30));
    fl_push_clip(x1 + 8, top + 3, std::max(0, width - (close ? 29 : 16)), height - 5);
    fl_draw(page->label() ? page->label() : "", x1 + 8, top + 2, std::max(0, width - (close ? 29 : 16)),
            height - 3, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    fl_pop_clip();
    if (close)
    {
        const int cx = x1 + width - 13, cy = top + height / 2;
        fl_line(cx - 3, cy - 3, cx + 3, cy + 3);
        fl_line(cx - 3, cy + 3, cx + 3, cy - 3);
    }
    if (selected && Fl::focus() == this)
    {
        fl_line_style(FL_DOT);
        fl_rect(x1 + 4, top + 5, width - 8, height - 9);
        fl_line_style(0);
    }
}

int Tabs::hit_close(Fl_Widget *page, int x, int y)
{
    tab_positions();
    for (int i = 0; i < children(); ++i)
    {
        if (child(i) != page || (tab_flags[i] & 1))
            continue;
        const int right = this->x() + tab_pos[i] + tab_offset + tab_width[i];
        return x >= right - 23 && x < right - 3 && y >= this->y() && y < this->y() + 30;
    }
    return 0;
}
} // namespace fltk_dock
