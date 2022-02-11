/*
    SPDX-FileCopyrightText: 2021 Francesco Sorrentino <francesco.sorr@gmail.com>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "input_method.h"

#include "base/wayland/server.h"
#include "input/filters/keyboard_grab.h"
#include "input/redirect.h"
#include "input/xkb/helpers.h"
#include "render/compositor.h"
#include "win/scene.h"
#include "win/transient.h"
#include "win/wayland/popup_placement.h"
#include "win/wayland/space.h"
#include "win/wayland/window.h"
#include "win/wayland/window_release.h"

#include <Wrapland/Server/input_method_v2.h>
#include <Wrapland/Server/seat.h>
#include <Wrapland/Server/surface.h>
#include <Wrapland/Server/text_input_pool.h>
#include <Wrapland/Server/text_input_v3.h>

#include <cassert>

namespace
{
using namespace KWin;

auto get_window(Wrapland::Server::text_input_v3* text_input)
{
    auto input_surface = text_input->entered_surface();
    auto window = workspace()->findAbstractClient(
        [input_surface](auto win) { return win->surface() == input_surface; });
    assert(window);
    return window;
}

template<typename Win>
QRect get_input_popup_placement(Win* parent_window, QRect const& cursor_rectangle)
{
    using constraint_adjust = Wrapland::Server::XdgShellSurface::ConstraintAdjustment;

    auto const toplevel = win::lead_of_annexed_transient(parent_window);
    auto const& screen_bounds = Workspace::self()->clientArea(
        toplevel->control->fullscreen() ? FullScreenArea : PlacementArea, toplevel);

    auto const& text_area = cursor_rectangle.isValid() ? cursor_rectangle : QRect(0, 0, 0, 0);

    // Which size should we use? Let's use the same size as the text area.
    auto const& size = text_area.size();

    // Ideally, these depend on the direction of text input.
    auto const& anchor_edge = Qt::BottomEdge | Qt::RightEdge;
    auto const& gravity = Qt::BottomEdge | Qt::RightEdge;

    auto const adjustments
        = constraint_adjust::ResizeX | constraint_adjust::SlideX | constraint_adjust::FlipY;

    return win::wayland::get_popup_placement<Win>({parent_window,
                                                   screen_bounds,
                                                   text_area,
                                                   anchor_edge,
                                                   gravity,
                                                   size,
                                                   QPoint(),
                                                   adjustments});
}
}

namespace KWin::input::wayland
{

using Wrapland::Server::input_method_keyboard_grab_v2;
using Wrapland::Server::input_method_popup_surface_v2;

input_method::input_method(base::wayland::server* server)
{
    auto seat = server->seat();

    text_input_manager_v3 = server->display->createTextInputManagerV3();
    input_method_manager_v2 = server->display->createInputMethodManagerV2();

    QObject::connect(seat,
                     &Wrapland::Server::Seat::input_method_v2_changed,
                     this,
                     &input_method::input_method_v2_changed);
    QObject::connect(
        seat, &Wrapland::Server::Seat::text_input_v3_enabled_changed, this, [this](auto enabled) {
            if (enabled) {
                activate_filters();
                activate_popups();
            } else {
                deactivate();
            }
        });
}

input_method::~input_method() = default;

void input_method::input_method_v2_changed()
{
    QObject::disconnect(notifiers.popup_created);
    QObject::disconnect(notifiers.keyboard_grabbed);

    popups.clear();
    filters.clear();

    if (auto device = waylandServer()->seat()->get_input_method_v2()) {
        notifiers.popup_created
            = QObject::connect(device,
                               &Wrapland::Server::input_method_v2::popup_surface_created,
                               this,
                               &input_method::handle_popup_surface_created);
        notifiers.keyboard_grabbed
            = QObject::connect(device,
                               &Wrapland::Server::input_method_v2::keyboard_grabbed,
                               this,
                               &input_method::handle_keyboard_grabbed);
    }
}

void input_method::handle_keyboard_grabbed(input_method_keyboard_grab_v2* grab)
{
    auto xkb = xkb::get_primary_xkb_keyboard();
    auto filter = filters.emplace_back(new keyboard_grab(grab, xkb->keymap->raw)).get();

    QObject::connect(grab,
                     &Wrapland::Server::input_method_keyboard_grab_v2::resourceDestroyed,
                     kwinApp()->input->redirect.get(),
                     [this, filter] {
                         kwinApp()->input->redirect->uninstallInputEventFilter(filter);
                         remove_all_if(filters, [filter](auto&& f) { return f.get() == filter; });
                     });

    if (auto ti3 = waylandServer()->seat()->text_inputs().v3.text_input;
        ti3 && ti3->state().enabled) {
        kwinApp()->input->redirect->append_filter(filter);
    }
}

void input_method::activate_filters()
{
    for (auto const& filter : filters) {
        kwinApp()->input->redirect->append_filter(filter.get());
    }
}

void input_method::handle_popup_surface_created(input_method_popup_surface_v2* popup_surface)
{
    using win::wayland::window;

    auto popup = popups.emplace_back(new window(popup_surface->surface()));
    popup->input_method_popup = popup_surface;
    popup->transient()->annexed = true;
    popup->hidden = true;
    popup->set_layer(win::layer::notification);

    auto space = static_cast<win::wayland::space*>(workspace());
    space->m_windows.push_back(popup);

    QObject::connect(
        popup, &window::windowClosed, this, [this, popup] { remove_all(popups, popup); });

    QObject::connect(popup_surface,
                     &input_method_popup_surface_v2::resourceDestroyed,
                     popup,
                     [popup] { win::wayland::destroy_window(popup); });

    QObject::connect(
        popup->surface(), &Wrapland::Server::Surface::committed, popup, &window::handle_commit);
    QObject::connect(popup, &window::needsRepaint, render::compositor::self(), [popup] {
        render::compositor::self()->schedule_repaint(popup);
    });
    QObject::connect(
        popup, &window::frame_geometry_changed, popup, [](auto win, auto old_frame_geo) {
            if (!win->transient()->lead()) {
                return;
            }

            auto const& old_visible_geo = win::visible_rect(win, old_frame_geo);
            auto const& visible_geo = win::visible_rect(win, win->frameGeometry());

            win::lead_of_annexed_transient(win)->addLayerRepaint(
                old_visible_geo.united(visible_geo));

            if (old_visible_geo.size() != visible_geo.size()) {
                win->discard_quads();
            }
        });

    if (popup->readyForPainting()) {
        space->handle_window_added(popup);
    } else {
        QObject::connect(
            popup, &window::windowShown, space, &win::wayland::space::handle_wayland_window_shown);
    }

    if (auto text_input = waylandServer()->seat()->text_inputs().v3.text_input) {
        if (text_input->state().enabled) {
            auto parent_window = get_window(text_input);
            auto const& placement
                = get_input_popup_placement(parent_window, text_input->state().cursor_rectangle);

            parent_window->transient()->add_child(popup);
            popup->setFrameGeometry(placement);
            popup->hideClient(false);
        }
    }
}

void input_method::activate_popups()
{
    if (popups.empty()) {
        return;
    }

    auto text_input = waylandServer()->seat()->text_inputs().v3.text_input;
    auto parent_window = get_window(text_input);
    auto const placement
        = get_input_popup_placement(parent_window, text_input->state().cursor_rectangle);

    for (auto const& popup : popups) {
        parent_window->transient()->add_child(popup);
        popup->setFrameGeometry(placement);
        popup->hideClient(false);
    }
}

void input_method::deactivate()
{
    for (auto const& filter : filters) {
        kwinApp()->input->redirect->uninstallInputEventFilter(filter.get());
    }
    for (auto const& popup : popups) {
        popup->hideClient(true);
        popup->transient()->lead()->transient()->remove_child(popup);
    }
}

}