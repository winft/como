/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "window_release.h"

#include <como/win/activation.h>
#include <como/win/geo.h>
#include <como/win/screen.h>
#include <como/win/space_areas_helpers.h>
#include <como/win/stacking.h>
#include <como/win/transient.h>
#include <como/win/wayland/space_windows.h>

#include <como/base/platform_qobject.h>
#include <como/base/wayland/output.h>
#include <como/base/wayland/output_helpers.h>
#include <como/base/wayland/server.h>

#include <KScreenLocker/KsldApp>
#include <Wrapland/Server/layer_shell_v1.h>
#include <Wrapland/Server/surface.h>

namespace como::win::wayland
{

template<typename Win>
QRectF layer_surface_area(Win* win)
{
    auto layer_surf = win->layer_surface;
    auto output = layer_surf->output();
    assert(output);

    // TODO(romangg): Instead of using the output geometry we should use some Workspace::clientArea
    //                depending on the layer the surface is on.
    auto const output_geo = output->get_state().geometry;

    if (layer_surf->exclusive_zone() == 0) {
        auto output_geo_rect = output_geo.toRect();
        auto area = space_window_area(win->space, area_option::work, output_geo_rect.center(), 0);
        return area.intersected(output_geo_rect);
    }

    return output_geo;
}

template<typename Win>
QSize layer_surface_size(Win* win)
{
    auto layer_surf = win->layer_surface;
    auto const area_geo = layer_surface_area(win);

    auto size = layer_surf->size();
    auto margins = layer_surf->margins();
    if (size.width() == 0) {
        assert(layer_surf->anchor() & Qt::LeftEdge);
        assert(layer_surf->anchor() & Qt::RightEdge);
        size.setWidth(area_geo.width() - margins.left() - margins.right());
    }
    if (size.height() == 0) {
        assert(layer_surf->anchor() & Qt::TopEdge);
        assert(layer_surf->anchor() & Qt::BottomEdge);
        size.setHeight(area_geo.height() - margins.top() - margins.bottom());
    }
    return size;
}

template<typename Win>
QPoint layer_surface_position(Win* win, QSize const& surface_size)
{
    auto layer_surf = win->layer_surface;
    auto const area_geo = layer_surface_area(win);

    auto anchor = layer_surf->anchor();
    auto margins = layer_surf->margins();

    auto x_mid = area_geo.x() + area_geo.width() / 2;
    auto y_mid = area_geo.y() + area_geo.height() / 2;

    // When not anchored to opposite edges or to both we center the surface in this dimension.
    auto x = x_mid - surface_size.width() / 2;
    auto y = y_mid - surface_size.height() / 2;

    if (anchor & Qt::LeftEdge) {
        if (!(anchor & Qt::RightEdge)) {
            // Anchored only left. We use the output position plus margin.
            x = area_geo.x() + margins.left();
        }
    } else if (anchor & Qt::RightEdge) {
        // Only anchored right. We position it relative to right output side minus margin.
        x = area_geo.right() - margins.right() - surface_size.width();
    }
    if (anchor & Qt::TopEdge) {
        if (!(anchor & Qt::BottomEdge)) {
            // Anchored only top. We use the output position plus margin.
            y = area_geo.y() + margins.top();
        }
    } else if (anchor & Qt::BottomEdge) {
        // Only anchored bottom. We position it relative to output bottom minus margin.
        y = area_geo.bottom() - margins.bottom() - surface_size.height();
    }
    return QPoint(x, y);
}

template<typename Win>
QRect layer_surface_recommended_geometry(Win* win)
{
    auto size = layer_surface_size(win);
    return QRect(layer_surface_position(win, size), size);
}

template<typename Win>
QRect layer_surface_requested_geometry(Win* win)
{
    auto const area_geo = layer_surface_area(win);
    auto const set_size = layer_surface_size(win);

    auto get_effective_size = [&] {
        auto size = set_size;

        if (auto const surface_size = win->surface->size(); !surface_size.isEmpty()) {
            // The client might have committed a surface with different size than the set_size.
            size.setWidth(std::min(set_size.width(), surface_size.width()));
            size.setHeight(std::min(set_size.height(), surface_size.height()));
        }

        // Limit to the output size.
        size.setHeight(std::min(size.height(), static_cast<int>(area_geo.height())));
        size.setWidth(std::min(size.width(), static_cast<int>(area_geo.width())));
        return size;
    };

    auto const eff_size = get_effective_size();
    return QRect(layer_surface_position(win, eff_size), eff_size);
}

template<typename Win>
void assign_layer_surface_role(Win* win, Wrapland::Server::LayerSurfaceV1* layer_surface)
{
    namespace WS = Wrapland::Server;

    assert(win->surface);
    assert(layer_surface->surface() == win->surface);

    win->control = std::make_unique<typename Win::layer_control_t>(*win);
    win->layer_surface = layer_surface;
    block_geometry_updates(win, true);

    QObject::connect(win->qobject.get(),
                     &Win::qobject_t::needsRepaint,
                     win->space.base.mod.render->qobject.get(),
                     [win] { win->space.base.mod.render->schedule_repaint(win); });
    QObject::connect(layer_surface,
                     &WS::LayerSurfaceV1::resourceDestroyed,
                     win->qobject.get(),
                     [win] { destroy_window(win); });

    QObject::connect(
        layer_surface, &WS::LayerSurfaceV1::got_popup, win->qobject.get(), [win](auto popup) {
            for (auto window : win->space.windows) {
                if (std::visit(overload{[&](Win* candidate) {
                                            if (candidate->popup == popup) {
                                                win->transient->add_child(candidate);
                                                return true;
                                            }
                                            return false;
                                        },
                                        [](auto&&) { return false; }},
                               window)) {
                    break;
                }
            }
        });

    QObject::connect(layer_surface,
                     &WS::LayerSurfaceV1::configure_acknowledged,
                     win->qobject.get(),
                     [win](auto serial) { win->acked_configure = serial; });

    auto handle_first_commit = [win] {
        QObject::disconnect(win->surface, &WS::Surface::committed, win->qobject.get(), nullptr);
        QObject::connect(win->surface, &WS::Surface::committed, win->qobject.get(), [win] {
            win->handle_commit();
        });

        block_geometry_updates(win, false);

        if (!win->layer_surface->output()) {
            if (auto output = get_current_output(win->space)) {
                win->layer_surface->set_output(output->wrapland_output());
            }
        }

        if (!base::wayland::find_output(win->space.base, win->layer_surface->output())) {
            // Output not found. Close surface and ignore.
            win->layer_surface->close();
            return;
        }

        QObject::connect(win->space.base.qobject.get(),
                         &base::platform_qobject::topology_changed,
                         win->qobject.get(),
                         [win] {
                             auto geo = layer_surface_recommended_geometry(win);
                             if (win->geo.update.frame != geo) {
                                 win->setFrameGeometry(geo);
                             }
                         });

        if (win->pending_configures.empty()) {
            // wlr-layer-shell protocol stipulates a single configure event on first commit.
            process_layer_surface_commit(win);
        }

        win->initialized = true;
    };

    QObject::connect(
        win->surface, &WS::Surface::committed, win->qobject.get(), handle_first_commit);
}

template<typename Window, typename Space>
void handle_new_layer_surface(Space* space, Wrapland::Server::LayerSurfaceV1* layer_surface)
{
    using var_win = typename Space::window_t;
    auto window = new Window(layer_surface->surface(), *space);

    if (layer_surface->surface()->client() == space->base.server->screen_locker_client_connection) {
        ScreenLocker::KSldApp::self()->lockScreenShown();
    }

    space->windows.push_back(window);
    QObject::connect(layer_surface,
                     &Wrapland::Server::LayerSurfaceV1::resourceDestroyed,
                     space->qobject.get(),
                     [space, window] { remove_all(space->windows, var_win(window)); });

    win::wayland::assign_layer_surface_role(window, layer_surface);

    if (window->render_data.ready_for_painting) {
        space_windows_add(*space, *window);
    }
}

template<typename Win>
void layer_surface_handle_keyboard_interactivity(Win* win)
{
    using var_win = typename Win::space_t::window_t;
    using inter = Wrapland::Server::LayerSurfaceV1::KeyboardInteractivity;

    auto interactivity = win->layer_surface->keyboard_interactivity();
    if (interactivity != inter::OnDemand) {
        if (auto act = most_recently_activated_window(win->space); act && *act == var_win(win)) {
            // With interactivity None or Exclusive just reset control.
            activate_next_window(win->space);
        }
    }
    win->space.input->keyboard->update();
}

template<typename Win>
void layer_surface_handle_exclusive_zone(Win* win)
{
    auto surf = win->layer_surface;
    auto exclusive = surf->exclusive_zone();

    if (exclusive <= 0) {
        // No edge is being reserved.
        return;
    }

    auto anchor = surf->anchor();

    // TODO(romangg): At the moment we reserve only via the clientArea mechanism in Workspace. This
    // is not fully compliant with layer-shell protocol, which allows to reserve space by integer.
    if ((anchor & Qt::LeftEdge) != (anchor & Qt::RightEdge)) {
        // Reference anchor is a vertical edge.
        if (anchor & Qt::LeftEdge) {
            // TODO(romangg): set strut explicitly on left edge.
        } else {
            // TODO(romangg): set strut explicitly on right edge.
        }
    } else {
        // Reference anchor is a horizontal edge.
        if (anchor & Qt::TopEdge) {
            // TODO(romangg): set strut explicitly on top edge.
        } else {
            // TODO(romangg): set strut explicitly on bottom edge.
        }
    }

    // TODO(romangg): Reserver space for screen edge?
}

template<typename Win>
win_type layer_surface_type(Win* win)
{
    using layer = Wrapland::Server::LayerSurfaceV1::Layer;
    switch (win->layer_surface->layer()) {
    case layer::Background:
        return win_type::desktop;
    case layer::Bottom:
        return win_type::dock;
    case layer::Top:
        return win_type::notification;
    case layer::Overlay:
        return win_type::on_screen_display;
    default:
        assert(false);
        return win_type::normal;
    }
}

template<typename Win>
void process_layer_surface_commit(Win* win)
{
    layer_surface_handle_keyboard_interactivity(win);

    auto geo = layer_surface_requested_geometry(win);
    layer_surface_handle_exclusive_zone(win);

    if (win->pending_configures.empty()) {
        win->setFrameGeometry(geo);
    } else {
        for (auto& config : win->pending_configures) {
            if (config.serial == win->acked_configure) {
                config.geometry.frame = geo;
                break;
            }
        }
    }

    win->window_type = layer_surface_type(win);

    if (win->layer_surface->layer() == Wrapland::Server::LayerSurfaceV1::Layer::Bottom) {
        win->control->keep_below = true;
    } else {
        win->control->keep_below = false;
    }

    if (win->window_type == win_type::desktop || win->window_type == win_type::on_screen_display
        || win->window_type == win_type::notification) {
        set_on_all_subspaces(*win, true);
    }

    update_layer(win);

    // TODO(romangg): update client area also on size change?
    if (win->layer_surface->exclusive_zone() > 0) {
        win::update_space_areas(win->space);
    }
}

template<typename Win>
void handle_layer_surface_commit(Win* win)
{
    assert(win->layer_surface);

    if (!win->layer_surface->change_pending() && win->geo.update.frame == win->geo.frame) {
        return;
    }

    process_layer_surface_commit(win);
}

}
