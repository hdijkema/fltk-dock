#include "fltk_dock.h"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace fltk_dock
{
namespace
{
constexpr int title_height = 28;
constexpr int splitter_width = 5;
bool horizontal(Side side)
{
    return side == Side::Left || side == Side::Right;
}
bool valid(Side side)
{
    return side == Side::Left || side == Side::Right || side == Side::Top || side == Side::Bottom;
}

// Keep restored floating windows inside a real work area, including after monitor changes.
void screen_rectangle(PanelState &state)
{
    int x, y, w, h;
    Fl::screen_work_area(x, y, w, h, state.x, state.y);
    state.w = std::clamp(state.w, 160, std::max(160, w));
    state.h = std::clamp(state.h, 100, std::max(100, h - 40));
    state.x = std::clamp(state.x, x, x + std::max(0, w - state.w));
    state.y = std::clamp(state.y, y + 30, y + std::max(30, h - state.h));
}
} // namespace

Panel::Panel(Host &host, std::string id, const char *title, Fl_Widget &content, Side side, int extent)
    : Fl_Group(host.x(), host.y(), 240, 300), host_(host), content_(&content)
{
    copy_label(title);
    align(FL_ALIGN_INSIDE);
    state_.id = std::move(id);
    state_.side = side;
    state_.extent = extent;
    end();
    Fl_Group::add(content);
    box(FL_FLAT_BOX);
    color(FL_BACKGROUND_COLOR);
}

Panel::~Panel()
{
    if (floating_)
    {
        floating_->remove(*this);
        floating_->hide();
        delete floating_;
    }
}

void Panel::resize(int x, int y, int w, int h)
{
    Fl_Widget::resize(x, y, w, h);
    content_->resize(x + 1, y + title_height, std::max(1, w - 2), std::max(1, h - title_height - 1));
    init_sizes();
    redraw();
}

// Paint a desktop title strip around existing content; the right controls float/dock and hide the pane.
void Panel::draw()
{
    draw_box();
    fl_color(color());
    fl_rectf(x(), y(), w(), title_height);
    fl_color(fl_color_average(color(), labelcolor(), 0.7f));
    fl_rect(x(), y(), w(), h());
    fl_color(labelcolor());
    fl_font(labelfont(), labelsize());
    fl_push_clip(x() + 8, y(), std::max(0, w() - 62), title_height);
    fl_draw(label() ? label() : "", x() + 8, y(), w() - 64, title_height, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    fl_pop_clip();
    const int right = x() + w() - 14, middle = y() + title_height / 2;
    fl_line(right - 3, middle - 3, right + 3, middle + 3);
    fl_line(right - 3, middle + 3, right + 3, middle - 3);
    fl_rect(right - 28, middle - 4, 9, 8);
    if (!is_floating())
        fl_line(right - 25, middle - 7, right - 16, middle - 7);
    draw_children();
}

// Context actions use the same ownership/placement operations as drag-and-drop.
void Panel::menu()
{
    Fl_Menu_Item items[] = {{"Dock left"}, {"Dock right"}, {"Dock top"}, {"Dock bottom"},
                            {"Float"},     {"Hide"},       {nullptr}};
    const auto *chosen = items->popup(Fl::event_x(), Fl::event_y());
    if (!chosen)
        return;
    const auto action = chosen - items;
    if (action < 4)
        host_.dock(*this, static_cast<Side>(action));
    else if (action == 4)
        host_.float_panel(*this, Fl::event_x_root() - 50, Fl::event_y_root() - 14);
    else
        host_.open(*this, false);
}

// Delay detaching a docked pane until release, so the original native window retains mouse capture.
// During a drag Host paints the target; a center drop joins tabs and an edge drop docks the pane.
int Panel::handle(int event)
{
    const bool title = Fl::event_y() >= y() && Fl::event_y() < y() + title_height;
    if (event == FL_PUSH && title)
    {
        if (Fl::event_button() == FL_RIGHT_MOUSE)
        {
            menu();
            return 1;
        }
        if (Fl::event_button() != FL_LEFT_MOUSE)
            return Fl_Group::handle(event);
        if (Fl::event_clicks())
        {
            if (is_floating())
                host_.dock(*this, state_.side);
            else
                host_.float_panel(*this, Fl::event_x_root() - 50, Fl::event_y_root() - 14);
            return 1;
        }
        press_x_ = Fl::event_x_root();
        press_y_ = Fl::event_y_root();
        offset_x_ = Fl::event_x() - x();
        offset_y_ = Fl::event_y() - y();
        dragging_ = false;
        pressed_button_ = Fl::event_x() >= x() + w() - 26 ? 2 : Fl::event_x() >= x() + w() - 52 ? 1 : 0;
        return 1;
    }
    if (event == FL_DRAG && Fl::pushed() == this)
    {
        const int distance =
            std::abs(Fl::event_x_root() - press_x_) + std::abs(Fl::event_y_root() - press_y_);
        if (!pressed_button_ && distance > 6)
            dragging_ = true;
        if (dragging_)
        {
            if (is_floating())
                floating_->position(Fl::event_x_root() - offset_x_, Fl::event_y_root() - offset_y_);
            host_.preview(Fl::event_x_root(), Fl::event_y_root(), this);
        }
        return 1;
    }
    if (event == FL_RELEASE)
    {
        if (dragging_)
        {
            dragging_ = false;
            host_.finish_drag(*this, Fl::event_x_root(), Fl::event_y_root(), offset_x_, offset_y_);
            return 1;
        }
        const int button = Fl::event_x() >= x() + w() - 26 ? 2 : Fl::event_x() >= x() + w() - 52 ? 1 : 0;
        if (title && pressed_button_ == button && button)
        {
            pressed_button_ = 0;
            if (button == 2)
                host_.open(*this, false);
            else if (is_floating())
                host_.dock(*this, state_.side);
            else
                host_.float_panel(*this, Fl::event_x_root() - 50, Fl::event_y_root() - 14);
            return 1;
        }
        pressed_button_ = 0;
    }
    return Fl_Group::handle(event);
}

Host::Host(int x, int y, int w, int h) : Fl_Group(x, y, w, h)
{
    box(FL_FLAT_BOX);
    color(fl_rgb_color(240, 240, 240));
    end();
}

Host::~Host()
{
    stop_preview();
    // Floating panes are not physical children of this group, but still belong to this Host.
    for (auto *panel : panels_)
        if (panel->parent() == panel->floating_)
            delete panel;
}

void Host::center(Fl_Widget &widget)
{
    if (center_ && center_ != &widget)
        throw std::invalid_argument("Dock host already has a central widget");
    center_ = &widget;
    Fl_Group::add(widget);
    layout();
}

Panel &Host::add_panel(std::string id, const char *title, Fl_Widget &widget, Side side, int extent)
{
    if (id.empty() || !valid(side) || extent < 80 || extent > 4096)
        throw std::invalid_argument(
            "Dock panels require an ID, a valid side and an extent between 80 and 4096");
    for (auto *panel : panels_)
        if (panel->id() == id || &panel->content() == &widget)
            throw std::invalid_argument("Dock panel IDs and content widgets must be unique");
    auto *previous = Fl_Group::current();
    Fl_Group::current(nullptr);
    auto *panel = new Panel(*this, std::move(id), title, widget, side, extent);
    Fl_Group::current(previous);
    Fl_Group::add(panel);
    panels_.push_back(panel);
    layout();
    return *panel;
}

void Host::dock(Panel &panel, Side side)
{
    if (&panel.host_ != this || !valid(side))
        throw std::invalid_argument("Dock target must be a side of the panel's own host");
    if (panel.floating_)
        panel.floating_->hide();
    Fl_Group::add(panel);
    panel.state_.side = side;
    panel.state_.floating = false;
    panel.state_.tab_group.clear();
    panel.state_.open = true;
    panel.show();
    layout();
}

// Edge strips are laid out outside-in. Move a complete target group as one insertion boundary.
void Host::reorder(Panel &panel, Panel *target, bool before)
{
    panels_.erase(std::find(panels_.begin(), panels_.end(), &panel));
    auto position = before ? panels_.begin() : panels_.end();
    if (target)
    {
        const auto group = target->state_.tab_group;
        auto matches = [&](Panel *p)
        { return p == target || (!group.empty() && p->state_.tab_group == group); };
        if (before)
            position = std::find_if(panels_.begin(), panels_.end(), matches);
        else
        {
            auto last = std::find_if(panels_.rbegin(), panels_.rend(), matches);
            position = last == panels_.rend() ? panels_.end() : last.base();
        }
    }
    panels_.insert(position, &panel);
}

void Host::dock_beside(Panel &panel, Panel &target, Side position)
{
    if (&panel.host_ != this || &target.host_ != this || &panel == &target || !valid(position) ||
        horizontal(position) != horizontal(target.side()) || !target.is_open() || target.is_floating())
        throw std::invalid_argument("Dock beside requires a visible strip and a parallel direction");
    const auto target_side = target.side();
    dock(panel, target_side);
    reorder(panel, &target, position == target_side);
    layout();
}

// A tabbed area is an ordinary Fl_Tabs-derived group. It owns its docked pages until they move away.
void Host::tabify(Panel &panel, Panel &target)
{
    if (&panel.host_ != this || &target.host_ != this)
        throw std::invalid_argument("Tabbed docking requires panels from the same host");
    if (&panel == &target)
        return;
    if (target.is_floating())
        dock(target, target.side());
    std::string group_id = target.state_.tab_group;
    Tabs *tabs = nullptr;
    for (auto &group : groups_)
        if ((!group_id.empty() && group.id == group_id) || (group_id.empty() && group.tabs->children() == 0))
        {
            tabs = group.tabs;
            group_id = group.id;
            break;
        }
    if (!tabs)
    {
        if (group_id.empty())
            group_id = target.id() + ":" + std::to_string(groups_.size());
        auto *previous = Fl_Group::current();
        Fl_Group::current(nullptr);
        tabs = new Tabs(x(), y(), 240, 300);
        tabs->end();
        Fl_Group::current(previous);
        Fl_Group::add(tabs);
        groups_.push_back({group_id, tabs});
    }
    dock(panel, target.side());
    target.state_.open = true;
    target.state_.tab_group = group_id;
    panel.state_.tab_group = group_id;
    panel.state_.extent = target.state_.extent;
    tabs->add(target);
    tabs->add(panel);
    tabs->value(&panel);
    layout();
}

void Host::float_panel(Panel &panel, int x, int y)
{
    if (&panel.host_ != this)
        throw std::invalid_argument("Floating panel belongs to a different host");
    if (!panel.state_.floating)
    {
        panel.state_.w = std::max(240, panel.w());
        panel.state_.h = std::max(180, panel.h());
    }
    panel.state_.x = x;
    panel.state_.y = y;
    screen_rectangle(panel.state_);
    if (!panel.floating_)
    {
        auto *previous = Fl_Group::current();
        Fl_Group::current(nullptr);
        panel.floating_ = new Fl_Double_Window(panel.state_.w, panel.state_.h);
        panel.floating_->end();
        Fl_Group::current(previous);
        panel.floating_->size_range(160, 100);
        panel.floating_->callback(
            [](Fl_Widget *, void *p)
            {
                auto &frame = *static_cast<Panel *>(p);
                frame.host_.open(frame, false);
            },
            &panel);
    }
    panel.floating_->add(panel);
    panel.resize(0, 0, panel.state_.w, panel.state_.h);
    panel.floating_->resizable(panel);
    panel.floating_->resize(panel.state_.x, panel.state_.y, panel.state_.w, panel.state_.h);
    panel.floating_->copy_label(panel.label());
    panel.state_.floating = true;
    panel.state_.open = true;
    panel.state_.tab_group.clear();
    panel.show();
    if (window() && window()->shown())
        panel.floating_->show();
    layout();
}

void Host::open(Panel &panel, bool visible)
{
    if (&panel.host_ != this)
        throw std::invalid_argument("Panel belongs to a different host");
    panel.state_.open = visible;
    if (!panel.state_.floating && !panel.state_.tab_group.empty())
    {
        if (!visible)
            Fl_Group::add(panel);
        else
            for (auto &group : groups_)
                if (group.id == panel.state_.tab_group)
                {
                    group.tabs->add(panel);
                    group.tabs->value(&panel);
                }
    }
    if (visible)
        panel.show();
    else
        panel.hide();
    if (panel.state_.floating && panel.floating_)
    {
        if (visible && window() && window()->shown())
            panel.floating_->show();
        else
            panel.floating_->hide();
    }
    layout();
}

std::vector<PanelState> Host::save_layout() const
{
    std::vector<PanelState> result;
    for (auto *panel : panels_)
    {
        auto state = panel->state_;
        if (state.floating && panel->floating_)
        {
            state.x = panel->floating_->x();
            state.y = panel->floating_->y();
            state.w = panel->floating_->w();
            state.h = panel->floating_->h();
        }
        if (auto *tabs = dynamic_cast<Tabs *>(panel->parent()))
            state.active = tabs->value() == panel;
        result.push_back(std::move(state));
    }
    return result;
}

// Validate first so malformed snapshots leave the live layout untouched, then restore by stable IDs.
void Host::restore_layout(const std::vector<PanelState> &states)
{
    std::set<std::string> ids;
    for (const auto &state : states)
    {
        if (state.id.empty() || !ids.insert(state.id).second || !valid(state.side) || state.extent < 80 ||
            state.extent > 4096 || state.w < 1 || state.h < 1 || state.w > 32768 || state.h > 32768 ||
            state.tab_group.size() > 256 || std::abs(static_cast<long long>(state.x)) > 1000000 ||
            std::abs(static_cast<long long>(state.y)) > 1000000)
            throw std::invalid_argument("Invalid dock layout snapshot");
    }
    std::vector<Panel *> order;
    for (const auto &state : states)
        for (auto *panel : panels_)
            if (panel->id() == state.id)
                order.push_back(panel);
    for (auto *panel : panels_)
        if (std::find(order.begin(), order.end(), panel) == order.end())
            order.push_back(panel);
    panels_ = std::move(order);
    std::map<std::string, Panel *> grouped;
    for (const auto &state : states)
        for (auto *panel : panels_)
            if (panel->id() == state.id)
            {
                dock(*panel, state.side);
                panel->state_ = state;
                panel->state_.tab_group.clear();
                if (state.floating)
                    float_panel(*panel, state.x, state.y);
                else if (!state.tab_group.empty())
                {
                    auto *&first = grouped[state.tab_group];
                    if (first)
                        tabify(*panel, *first);
                    else
                        first = panel;
                }
            }
    for (const auto &state : states)
        for (auto *panel : panels_)
            if (panel->id() == state.id)
                open(*panel, state.open);
    for (const auto &state : states)
        for (auto *panel : panels_)
            if (panel->id() == state.id && state.active && state.open && !panel->state_.tab_group.empty())
                static_cast<Tabs *>(panel->parent())->value(panel);
    layout();
}

// Reserve edge strips in registration order. Grouped panels share one strip and one splitter.
// The central widget always gets the remaining rectangle, even while panels are hidden/floating.
void Host::layout()
{
    int left = x(), top = y(), width = w(), height = h();
    splitters_.clear();
    std::set<std::string> laid_out;
    for (auto &group : groups_)
        group.tabs->hide();
    for (auto *panel : panels_)
    {
        if (!panel->state_.open || panel->state_.floating)
            continue;
        Fl_Widget *frame = panel;
        const auto &group_id = panel->state_.tab_group;
        if (!group_id.empty())
        {
            if (!laid_out.insert(group_id).second)
                continue;
            frame = panel->parent();
            frame->show();
            auto *tabs = static_cast<Tabs *>(frame);
            auto *selected = static_cast<Panel *>(tabs->value());
            if (!selected || !selected->is_open())
                tabs->value(panel);
        }
        const auto side = panel->state_.side;
        const int available = horizontal(side) ? width - 180 : height - 120;
        const int extent = std::clamp(panel->state_.extent, 0, std::max(0, available - splitter_width));
        int px = left, py = top, pw = width, ph = height;
        Splitter splitter{panel, left, top, width, height};
        if (side == Side::Left)
        {
            pw = extent;
            splitter.x = left + extent;
            splitter.w = splitter_width;
            left += extent + splitter_width;
            width -= extent + splitter_width;
        }
        else if (side == Side::Right)
        {
            px = left + width - extent;
            pw = extent;
            splitter.x = px - splitter_width;
            splitter.w = splitter_width;
            width -= extent + splitter_width;
        }
        else if (side == Side::Top)
        {
            ph = extent;
            splitter.y = top + extent;
            splitter.h = splitter_width;
            top += extent + splitter_width;
            height -= extent + splitter_width;
        }
        else
        {
            py = top + height - extent;
            ph = extent;
            splitter.y = py - splitter_width;
            splitter.h = splitter_width;
            height -= extent + splitter_width;
        }
        frame->resize(px, py, std::max(1, pw), std::max(1, ph));
        splitters_.push_back(splitter);
    }
    if (center_)
        center_->resize(left, top, std::max(1, width), std::max(1, height));
    init_sizes();
    redraw();
}

void Host::resize(int x, int y, int w, int h)
{
    Fl_Widget::resize(x, y, w, h);
    layout();
}

namespace
{
constexpr int guide_size = 28, guide_step = 34, edge_offset = 22;
bool hit(int px, int py, int cx, int cy)
{
    return px >= cx - guide_size / 2 && px < cx + guide_size / 2 && py >= cy - guide_size / 2 &&
           py < cy + guide_size / 2;
}
void guide(int cx, int cy, int direction, bool selected, double opacity, Fl_Color background)
{
    const auto blend = [&](Fl_Color color)
    { return fl_color_average(color, background, static_cast<float>(opacity)); };
    fl_color(blend(selected ? FL_SELECTION_COLOR : fl_color_average(FL_SELECTION_COLOR, background, 0.1f)));
    fl_rectf(cx - guide_size / 2, cy - guide_size / 2, guide_size, guide_size);
    fl_color(blend(FL_SELECTION_COLOR));
    fl_rect(cx - guide_size / 2, cy - guide_size / 2, guide_size, guide_size);
    fl_color(blend(selected ? fl_contrast(FL_WHITE, FL_SELECTION_COLOR) : FL_SELECTION_COLOR));
    if (direction == 4)
    {
        fl_rect(cx - 8, cy - 6, 16, 12);
        fl_line(cx - 7, cy - 2, cx + 7, cy - 2);
        fl_line(cx - 2, cy - 5, cx - 2, cy - 2);
    }
    else if (direction == 0)
        fl_polygon(cx - 7, cy, cx + 4, cy - 7, cx + 4, cy + 7);
    else if (direction == 1)
        fl_polygon(cx + 7, cy, cx - 4, cy - 7, cx - 4, cy + 7);
    else if (direction == 2)
        fl_polygon(cx, cy - 7, cx - 7, cy + 4, cx + 7, cy + 4);
    else
        fl_polygon(cx, cy + 7, cx - 7, cy - 4, cx + 7, cy - 4);
}
} // namespace

Panel *Host::drop_panel(int root_x, int root_y, const Panel *moving) const
{
    if (!window())
        return nullptr;
    const int px = root_x - window()->x_root(), py = root_y - window()->y_root();
    for (auto *panel : panels_)
        if (panel != moving && panel->is_open() && !panel->is_floating() && panel->visible_r())
        {
            auto *frame = panel->state_.tab_group.empty() ? static_cast<Fl_Widget *>(panel) : panel->parent();
            if (px >= frame->x() && px < frame->x() + frame->w() && py >= frame->y() &&
                py < frame->y() + frame->h())
                return panel;
        }
    return nullptr;
}

Host::DropTarget Host::drop_target(int root_x, int root_y, const Panel *moving) const
{
    if (!window())
        return {};
    const int px = root_x - window()->x_root(), py = root_y - window()->y_root();
    if (px < x() || py < y() || px >= x() + w() || py >= y() + h())
        return {};
    const int gx[] = {x() + edge_offset, x() + w() - edge_offset, x() + w() / 2, x() + w() / 2};
    const int gy[] = {y() + h() / 2, y() + h() / 2, y() + edge_offset, y() + h() - edge_offset};
    for (int side = 0; side < 4; ++side)
        if (hit(px, py, gx[side], gy[side]))
            return {true, false, static_cast<Side>(side), nullptr};
    // The narrow outer rim always inserts outside the existing strips, independent of registration order.
    const int distances[] = {px - x(), x() + w() - 1 - px, py - y(), y() + h() - 1 - py};
    int nearest = 0;
    for (int i = 1; i < 4; ++i)
        if (distances[i] < distances[nearest])
            nearest = i;
    if (distances[nearest] < 16)
        return {true, false, static_cast<Side>(nearest), nullptr};
    if (auto *target = drop_panel(root_x, root_y, moving))
    {
        auto *frame = target->state_.tab_group.empty() ? static_cast<Fl_Widget *>(target) : target->parent();
        const int cx = frame->x() + frame->w() / 2, cy = frame->y() + frame->h() / 2;
        const int first = horizontal(target->side()) ? 0 : 2;
        for (int side = first; side < first + 2; ++side)
        {
            const int dx = side == 0 ? -guide_step : side == 1 ? guide_step : 0;
            const int dy = side == 2 ? -guide_step : side == 3 ? guide_step : 0;
            if (hit(px, py, cx + dx, cy + dy))
                return {true, false, static_cast<Side>(side), target};
        }
        if (hit(px, py, cx, cy))
            return {true, true, target->side(), target};
        if (horizontal(target->side()))
        {
            if (px < frame->x() + 24)
                return {true, false, Side::Left, target};
            if (px >= frame->x() + frame->w() - 24)
                return {true, false, Side::Right, target};
        }
        else
        {
            if (py < frame->y() + 24)
                return {true, false, Side::Top, target};
            if (py >= frame->y() + frame->h() - 24)
                return {true, false, Side::Bottom, target};
        }
        // Other positions have no action: tabbing is an explicit center target.
        return {};
    }
    return {};
}

namespace
{
constexpr double animation_seconds = 0.14;
double progress(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point now)
{
    const double t =
        std::clamp(std::chrono::duration<double>(now - start).count() / animation_seconds, 0.0, 1.0);
    return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
}
} // namespace

Host::Rectangle Host::animated_rectangle(Clock::time_point now) const
{
    Rectangle result;
    const double t = progress(outline_started_, now);
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = outline_from_[i] + (outline_to_[i] - outline_from_[i]) * t;
    return result;
}

void Host::animate(void *data)
{
    auto &host = *static_cast<Host *>(data);
    host.animation_pending_ = false;
    if (!host.preview_)
        return;
    host.redraw();
    const auto now = Clock::now();
    if (progress(host.guides_started_, now) < 1.0 ||
        (host.guide_panel_ && progress(host.local_started_, now) < 1.0) ||
        (host.outline_ready_ && progress(host.outline_started_, now) < 1.0))
        host.start_animation();
}

void Host::start_animation()
{
    if (!animation_pending_)
    {
        animation_pending_ = true;
        Fl::add_timeout(1.0 / 60.0, animate, this);
    }
}

void Host::stop_preview()
{
    Fl::remove_timeout(animate, this);
    animation_pending_ = false;
    preview_ = false;
    outline_ready_ = false;
    moving_ = nullptr;
    guide_panel_ = nullptr;
}

void Host::preview(int x, int y, const Panel *moving)
{
    const auto now = Clock::now();
    if (!preview_)
        guides_started_ = now;
    auto *panel = drop_panel(x, y, moving);
    if (!preview_ || panel != guide_panel_)
        local_started_ = now;
    const auto target = drop_target(x, y, moving);
    if (target.valid)
    {
        const auto rectangle = target_rectangle(target, *moving);
        if (!outline_ready_ || rectangle != outline_to_)
        {
            outline_from_ = outline_ready_ ? animated_rectangle(now) : rectangle;
            outline_to_ = rectangle;
            outline_started_ = now;
        }
        outline_ready_ = true;
    }
    moving_ = moving;
    guide_panel_ = panel;
    preview_target_ = target;
    preview_ = true;
    start_animation();
    redraw();
}

void Host::finish_drag(Panel &panel, int root_x, int root_y, int offset_x, int offset_y)
{
    const auto target = drop_target(root_x, root_y, &panel);
    stop_preview();
    redraw();
    if (target.valid && target.tabs)
        tabify(panel, *target.panel);
    else if (target.valid && target.panel)
        dock_beside(panel, *target.panel, target.side);
    else if (target.valid)
    {
        dock(panel, target.side);
        reorder(panel, nullptr, true);
        layout();
    }
    else
        float_panel(panel, root_x - offset_x, root_y - offset_y);
}

Host::Rectangle Host::target_rectangle(const DropTarget &target, const Panel &moving) const
{
    int px = x(), py = y(), pw = w(), ph = h();
    int extent = moving.state_.extent;
    if (target.panel)
    {
        auto *frame = target.panel->state_.tab_group.empty() ? static_cast<Fl_Widget *>(target.panel)
                                                             : target.panel->parent();
        px = frame->x();
        py = frame->y();
        pw = frame->w();
        ph = frame->h();
        if (!target.tabs)
        {
            if (target.side == Side::Left)
            {
                px = target.panel->side() == Side::Left ? px : px - extent - splitter_width;
                pw = extent;
            }
            else if (target.side == Side::Right)
            {
                px = target.panel->side() == Side::Right ? px + pw - extent : px + pw + splitter_width;
                pw = extent;
            }
            else if (target.side == Side::Top)
            {
                py = target.panel->side() == Side::Top ? py : py - extent - splitter_width;
                ph = extent;
            }
            else
            {
                py = target.panel->side() == Side::Bottom ? py + ph - extent : py + ph + splitter_width;
                ph = extent;
            }
        }
    }
    else if (horizontal(target.side))
    {
        pw = std::min(extent, w() / 2);
        if (target.side == Side::Right)
            px = x() + w() - pw;
    }
    else
    {
        ph = std::min(extent, h() / 2);
        if (target.side == Side::Bottom)
            py = y() + h() - ph;
    }
    return {static_cast<double>(px), static_cast<double>(py), static_cast<double>(pw),
            static_cast<double>(ph)};
}

// The same hit regions drive the arrows, highlight and release action.
void Host::draw()
{
    Fl_Group::draw();
    if (!preview_ || !moving_)
        return;
    fl_push_clip(x(), y(), w(), h());
    const auto &target = preview_target_;
    const auto now = Clock::now();
    const double opacity = progress(guides_started_, now);
    if (target.valid)
    {
        const auto rectangle = animated_rectangle(now);
        const int px = static_cast<int>(std::lround(rectangle[0])),
                  py = static_cast<int>(std::lround(rectangle[1]));
        const int pw = static_cast<int>(std::lround(rectangle[2])),
                  ph = static_cast<int>(std::lround(rectangle[3]));
        fl_color(fl_color_average(FL_SELECTION_COLOR, color(), static_cast<float>(opacity)));
        fl_line_style(FL_SOLID, 3);
        fl_rect(px + 2, py + 2, std::max(1, pw - 4), std::max(1, ph - 4));
        fl_line_style(0);
    }
    const int gx[] = {x() + edge_offset, x() + w() - edge_offset, x() + w() / 2, x() + w() / 2};
    const int gy[] = {y() + h() / 2, y() + h() / 2, y() + edge_offset, y() + h() - edge_offset};
    for (int side = 0; side < 4; ++side)
        guide(gx[side], gy[side], side,
              target.valid && !target.panel && static_cast<int>(target.side) == side, opacity, color());
    if (guide_panel_)
    {
        auto *frame = guide_panel_->state_.tab_group.empty() ? static_cast<Fl_Widget *>(guide_panel_)
                                                             : guide_panel_->parent();
        const int cx = frame->x() + frame->w() / 2, cy = frame->y() + frame->h() / 2;
        guide(cx, cy, 4, target.valid && target.tabs && target.panel == guide_panel_,
              progress(local_started_, now), color());
        const int first = horizontal(guide_panel_->side()) ? 0 : 2;
        for (int side = first; side < first + 2; ++side)
        {
            const int dx = side == 0 ? -guide_step : side == 1 ? guide_step : 0;
            const int dy = side == 2 ? -guide_step : side == 3 ? guide_step : 0;
            guide(cx + dx, cy + dy, side,
                  target.valid && !target.tabs && target.panel == guide_panel_ &&
                      static_cast<int>(target.side) == side,
                  progress(local_started_, now), color());
        }
    }
    fl_pop_clip();
}

// Splitters resize one edge area; floating windows follow the visibility of the owning host.
int Host::handle(int event)
{
    if (event == FL_HIDE)
        stop_preview();
    if (event == FL_SHOW || event == FL_HIDE)
        for (auto *panel : panels_)
            if (panel->is_floating() && panel->is_open())
            {
                if (event == FL_SHOW)
                    panel->floating_->show();
                else
                    panel->floating_->hide();
            }
    if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE)
        for (const auto &splitter : splitters_)
            if (Fl::event_x() >= splitter.x && Fl::event_x() < splitter.x + splitter.w &&
                Fl::event_y() >= splitter.y && Fl::event_y() < splitter.y + splitter.h)
            {
                resizing_ = splitter.panel;
                resize_origin_ = horizontal(resizing_->side()) ? Fl::event_x() : Fl::event_y();
                resize_extent_ = resizing_->state_.extent;
                return 1;
            }
    if (event == FL_DRAG && resizing_)
    {
        int delta = (horizontal(resizing_->side()) ? Fl::event_x() : Fl::event_y()) - resize_origin_;
        if (resizing_->side() == Side::Right || resizing_->side() == Side::Bottom)
            delta = -delta;
        const int extent = std::clamp(resize_extent_ + delta, 80, 4096);
        for (auto *panel : panels_)
            if (panel == resizing_ || (!resizing_->state_.tab_group.empty() &&
                                       panel->state_.tab_group == resizing_->state_.tab_group))
                panel->state_.extent = extent;
        layout();
        return 1;
    }
    if (event == FL_RELEASE && resizing_)
    {
        resizing_ = nullptr;
        return 1;
    }
    if (event == FL_MOVE || event == FL_LEAVE)
    {
        for (const auto &splitter : splitters_)
            if (event == FL_MOVE && Fl::event_x() >= splitter.x && Fl::event_x() < splitter.x + splitter.w &&
                Fl::event_y() >= splitter.y && Fl::event_y() < splitter.y + splitter.h)
            {
                splitter_hover_ = true;
                if (window())
                    window()->cursor(horizontal(splitter.panel->side()) ? FL_CURSOR_WE : FL_CURSOR_NS);
                return 1;
            }
        // Reset only a cursor set by this Host. Child widgets own their own hover cursor.
        if (splitter_hover_)
        {
            splitter_hover_ = false;
            if (window())
                window()->cursor(FL_CURSOR_DEFAULT);
        }
    }
    return Fl_Group::handle(event);
}
} // namespace fltk_dock
