#pragma once
#include <FL/Fl_Group.H>
#include <FL/Fl_Double_Window.H>
#include <array>
#include <chrono>
#include <string>
#include <vector>
#include "fltk_dock_tabs.h"

namespace fltk_dock
{
enum class Side
{
    Left,
    Right,
    Top,
    Bottom
};

// Application-owned snapshot. IDs identify existing panels; unknown IDs are ignored on restore.
struct PanelState
{
    std::string id;
    Side side = Side::Left;
    int extent = 240;
    bool open = true, floating = false;
    int x = 100, y = 100, w = 400, h = 400;
    std::string tab_group;
    bool active = false;
};

class Host;

// Host-owned frame around one content widget. Drag its title to dock/float; double-click to toggle.
// All methods require the GUI thread and a live Host. The frame owns content and its floating window.
class Panel : public Fl_Group
{
  public:
    ~Panel() override;
    const std::string &id() const
    {
        return state_.id;
    }
    bool is_open() const
    {
        return state_.open;
    }
    bool is_floating() const
    {
        return state_.floating;
    }
    Side side() const
    {
        return state_.side;
    }
    Fl_Widget &content() const
    {
        return *content_;
    }
    void resize(int, int, int, int) override;
    int handle(int) override;

  protected:
    void draw() override;

  private:
    friend class Host;
    friend class Tabs;
    Panel(Host &, std::string id, const char *title, Fl_Widget &, Side, int extent);
    Host &host_;
    Fl_Widget *content_;
    Fl_Double_Window *floating_ = nullptr;
    PanelState state_;
    bool dragging_ = false;
    int press_x_ = 0, press_y_ = 0, offset_x_ = 0, offset_y_ = 0, pressed_button_ = 0;
    void menu();
};

// Own a central widget and ordered edge panels, including panels in floating windows.
// Use add_panel/center with heap widgets; do not delete registered panels independently.
// layout() lays out edge strips and splitters, then gives the remaining rectangle to center().
class Host : public Fl_Group
{
  public:
    Host(int, int, int, int);
    ~Host() override;
    // Reparent and own widget as the central content. Set it once, before interacting with panels.
    void center(Fl_Widget &);
    // Register a unique nonempty ID and transfer widget ownership to a titled panel.
    // Returns the Host-owned frame. Invalid IDs/extents throw std::invalid_argument.
    Panel &add_panel(std::string id, const char *title, Fl_Widget &, Side, int extent = 240);
    // Move an existing panel to an edge or floating window without recreating its content.
    void dock(Panel &, Side);
    // Insert beside an existing edge strip. Left/right strips accept Left or Right;
    // top/bottom strips accept Top or Bottom. Tab groups remain together.
    void dock_beside(Panel &, Panel &target, Side position);
    // Put panel in the same tabbed docking area as target; target must belong to this Host.
    // Moving/floating one tab keeps the other panels in their docking area.
    void tabify(Panel &, Panel &target);
    void float_panel(Panel &, int screen_x, int screen_y);
    // Hide/reopen an existing panel while retaining placement, content and floating geometry.
    void open(Panel &, bool = true);
    // Snapshot/restore registered panels by ID. Restore validates the whole input before changing it.
    // Floating rectangles are clamped to a screen; no files or application data are read or written.
    std::vector<PanelState> save_layout() const;
    void restore_layout(const std::vector<PanelState> &);
    void resize(int, int, int, int) override;
    void layout();
    int handle(int) override;

  protected:
    void draw() override;

  private:
    friend class Panel;
    friend class Tabs;
    struct Splitter
    {
        Panel *panel;
        int x, y, w, h;
    };
    struct TabGroup
    {
        std::string id;
        Tabs *tabs;
    };
    Fl_Widget *center_ = nullptr;
    std::vector<Panel *> panels_;
    std::vector<Splitter> splitters_;
    std::vector<TabGroup> groups_;
    Panel *resizing_ = nullptr;
    bool splitter_hover_ = false;
    int resize_origin_ = 0, resize_extent_ = 0;
    struct DropTarget
    {
        bool valid = false, tabs = false;
        Side side = Side::Left;
        Panel *panel = nullptr;
    };
    using Clock = std::chrono::steady_clock;
    using Rectangle = std::array<double, 4>;
    Rectangle outline_from_{}, outline_to_{};
    Clock::time_point guides_started_{}, local_started_{}, outline_started_{};
    bool outline_ready_ = false, animation_pending_ = false;
    static void animate(void *);
    void start_animation();
    void stop_preview();
    Rectangle target_rectangle(const DropTarget &, const Panel &) const;
    Rectangle animated_rectangle(Clock::time_point) const;
    bool preview_ = false;
    const Panel *moving_ = nullptr;
    Panel *guide_panel_ = nullptr;
    DropTarget preview_target_;
    DropTarget drop_target(int screen_x, int screen_y, const Panel *) const;
    void finish_drag(Panel &, int screen_x, int screen_y, int offset_x, int offset_y);
    void reorder(Panel &, Panel *target, bool before);
    Panel *drop_panel(int screen_x, int screen_y, const Panel *) const;
    void preview(int screen_x, int screen_y, const Panel *);
};
} // namespace fltk_dock
