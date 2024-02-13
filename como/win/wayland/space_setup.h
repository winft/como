/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <win/singleton_interface.h>
#include <win/space_setup.h>
#include <win/user_actions_menu.h>
#include <win/wayland/appmenu.h>
#include <win/wayland/deco.h>
#include <win/wayland/layer_shell.h>
#include <win/wayland/plasma_shell.h>
#include <win/wayland/plasma_window.h>
#include <win/wayland/setup.h>
#include <win/wayland/subspace_manager.h>
#include <win/wayland/window.h>
#include <win/wayland/xdg_activation.h>
#include <win/wayland/xdg_shell.h>
#include <xwl/surface.h>

#include <Wrapland/Server/appmenu.h>
#include <Wrapland/Server/compositor.h>
#include <Wrapland/Server/idle_inhibit_v1.h>
#include <Wrapland/Server/plasma_activation_feedback.h>
#include <Wrapland/Server/plasma_shell.h>
#include <Wrapland/Server/plasma_virtual_desktop.h>
#include <Wrapland/Server/server_decoration_palette.h>
#include <Wrapland/Server/subcompositor.h>
#include <Wrapland/Server/xdg_shell.h>

namespace como::win::wayland
{

template<typename Space>
void space_setup_handle_x11_window_added(Space& space, typename Space::x11_window* window)
{
    auto setup_plasma_management_for_x11 = [&space, window] {
        setup_plasma_management(&space, window);

        // X11 windows can be added to the stacking order before they are ready to be painted.
        // The stacking order changed update comes too early because of that. As a workaround
        // update the stacking order explicitly one more time here.
        // TODO(romangg): Can we add an X11 window late to the stacking order, i.e. once it's
        //                ready to be painted? This way we would not need this additional call.
        plasma_manage_update_stacking_order(space);
    };

    if (window->render_data.ready_for_painting) {
        setup_plasma_management_for_x11();
    } else {
        QObject::connect(window->qobject.get(),
                         &Space::x11_window::qobject_t::windowShown,
                         space.qobject.get(),
                         [setup_plasma_management_for_x11] { setup_plasma_management_for_x11(); });
    }
}

template<typename Space>
void space_setup_handle_subspace_removed(Space& space, subspace* sub)
{
    for (auto const& win : space.windows) {
        std::visit(overload{[&space, sub](auto&& win) {
                       if (!win->control || !contains(win->topo.subspaces, sub)) {
                           return;
                       }

                       if (win->topo.subspaces.size() > 1) {
                           leave_subspace(*win, sub);
                           return;
                       }
                       send_window_to_subspace(
                           space,
                           win,
                           qMin(sub->x11DesktopNumber(), space.subspace_manager->subspaces.size()),
                           true);
                   }},
                   win);
    }
}

template<typename Space, typename Render, typename Input>
void space_setup_init(Space& space, Render& render, Input& input)
{
    namespace WS = Wrapland::Server;
    using wayland_window = win::wayland::window<Space>;

    space.qobject
        = std::make_unique<space_qobject>([&space] { space_start_reconfigure_timer(space); });
    space.options = std::make_unique<win::options>(input.base.config.main);
    space.rule_book = std::make_unique<rules::book>();
    space.subspace_manager = std::make_unique<subspace_manager>();
    space.outline = Space::render_outline_t::create(
        render, [&space] { return space.outline->create_visual(*space.base.mod.render); });
    space.deco = std::make_unique<deco::bridge<Space>>(space);
    space.appmenu = std::make_unique<dbus::appmenu>(dbus::create_appmenu_callbacks(space));
    space.user_actions_menu = std::make_unique<win::user_actions_menu<Space>>(space);
    space.compositor
        = std::make_unique<Wrapland::Server::Compositor>(space.base.server->display.get());
    space.subcompositor
        = std::make_unique<Wrapland::Server::Subcompositor>(space.base.server->display.get());
    space.xdg_shell
        = std::make_unique<Wrapland::Server::XdgShell>(space.base.server->display.get());
    space.layer_shell
        = std::make_unique<Wrapland::Server::LayerShellV1>(space.base.server->display.get());
    space.xdg_decoration_manager = std::make_unique<Wrapland::Server::XdgDecorationManager>(
        space.base.server->display.get(), space.xdg_shell.get());
    space.xdg_foreign
        = std::make_unique<Wrapland::Server::XdgForeign>(space.base.server->display.get());
    space.plasma_activation_feedback
        = std::make_unique<Wrapland::Server::plasma_activation_feedback>(
            space.base.server->display.get());
    space.plasma_shell
        = std::make_unique<Wrapland::Server::PlasmaShell>(space.base.server->display.get());
    space.plasma_window_manager
        = std::make_unique<Wrapland::Server::PlasmaWindowManager>(space.base.server->display.get());
    space.plasma_subspace_manager = std::make_unique<Wrapland::Server::PlasmaVirtualDesktopManager>(
        space.base.server->display.get());
    space.idle_inhibit_manager_v1 = std::make_unique<Wrapland::Server::IdleInhibitManagerV1>(
        space.base.server->display.get());
    space.appmenu_manager
        = std::make_unique<Wrapland::Server::AppmenuManager>(space.base.server->display.get());
    space.server_side_decoration_palette_manager
        = std::make_unique<Wrapland::Server::ServerSideDecorationPaletteManager>(
            space.base.server->display.get());

    singleton_interface::get_current_output_geometry = [&space] {
        auto output = get_current_output(space);
        return output ? output->geometry() : QRect();
    };
    singleton_interface::set_activation_token
        = [&space](auto const& appid) { return xdg_activation_set_token(space, appid); };
    singleton_interface::create_internal_window = [&space](auto qwindow) {
        auto iwin = new Space::internal_window_t(qwindow, space);
        return iwin->singleton.get();
    };

    space.input = input.integrate_space(space);
    space.edges = std::make_unique<typename Space::edger_t>(space);

    space.plasma_window_manager->setShowingDesktopState(
        Wrapland::Server::PlasmaWindowManager::ShowingDesktopState::Disabled);
    space.plasma_window_manager->setVirtualDesktopManager(space.plasma_subspace_manager.get());
    setup_subspace_manager(*space.subspace_manager, *space.plasma_subspace_manager);

    QObject::connect(
        space.stacking.order.qobject.get(),
        &stacking_order_qobject::render_restack,
        space.qobject.get(),
        [&space] {
            for (auto win : space.windows) {
                std::visit(overload{[&](Space::internal_window_t* win) {
                                        if (win->isShown() && !win->remnant) {
                                            space.stacking.order.render_overlays.push_back(win);
                                        }
                                    },
                                    [](auto&&) {}},
                           win);
            }
        });

    QObject::connect(space.compositor.get(),
                     &WS::Compositor::surfaceCreated,
                     space.qobject.get(),
                     [&space](auto surface) { xwl::handle_new_surface(&space, surface); });

    QObject::connect(
        space.xdg_shell.get(),
        &WS::XdgShell::toplevelCreated,
        space.qobject.get(),
        [&space](auto toplevel) { handle_new_toplevel<wayland_window>(&space, toplevel); });
    QObject::connect(space.xdg_shell.get(),
                     &WS::XdgShell::popupCreated,
                     space.qobject.get(),
                     [&space](auto popup) { handle_new_popup<wayland_window>(&space, popup); });

    QObject::connect(space.xdg_decoration_manager.get(),
                     &WS::XdgDecorationManager::decorationCreated,
                     space.qobject.get(),
                     [&space](auto deco) { handle_new_xdg_deco(&space, deco); });

    space.xdg_activation = std::make_unique<wayland::xdg_activation<Space>>(space);
    QObject::connect(
        space.qobject.get(), &Space::qobject_t::clientActivated, space.qobject.get(), [&space] {
            if (space.stacking.active) {
                space.xdg_activation->clear();
            }
        });

    QObject::connect(space.plasma_shell.get(),
                     &WS::PlasmaShell::surfaceCreated,
                     space.qobject.get(),
                     [&space](auto surface) { handle_new_plasma_shell_surface(&space, surface); });

    QObject::connect(space.appmenu_manager.get(),
                     &WS::AppmenuManager::appmenuCreated,
                     [&space](auto appmenu) { handle_new_appmenu(&space, appmenu); });

    QObject::connect(space.server_side_decoration_palette_manager.get(),
                     &WS::ServerSideDecorationPaletteManager::paletteCreated,
                     [&space](auto palette) { handle_new_palette(&space, palette); });

    QObject::connect(space.plasma_window_manager.get(),
                     &WS::PlasmaWindowManager::requestChangeShowingDesktop,
                     space.qobject.get(),
                     [&space](auto state) { handle_change_showing_desktop(&space, state); });
    QObject::connect(space.qobject.get(),
                     &Space::qobject_t::showingDesktopChanged,
                     space.qobject.get(),
                     [&space](bool set) {
                         using ShowingState
                             = Wrapland::Server::PlasmaWindowManager::ShowingDesktopState;
                         space.plasma_window_manager->setShowingDesktopState(
                             set ? ShowingState::Enabled : ShowingState::Disabled);
                     });
    QObject::connect(space.stacking.order.qobject.get(),
                     &stacking_order_qobject::changed,
                     space.plasma_window_manager.get(),
                     [&space] { plasma_manage_update_stacking_order(space); });

    QObject::connect(
        space.subcompositor.get(),
        &WS::Subcompositor::subsurfaceCreated,
        space.qobject.get(),
        [&space](auto subsurface) { handle_new_subsurface<wayland_window>(&space, subsurface); });
    QObject::connect(space.layer_shell.get(),
                     &WS::LayerShellV1::surface_created,
                     space.qobject.get(),
                     [&space](auto layer_surface) {
                         handle_new_layer_surface<wayland_window>(&space, layer_surface);
                     });

    QObject::connect(
        space.subspace_manager->qobject.get(),
        &subspace_manager_qobject::subspace_removed,
        space.qobject.get(),
        [&space](auto&& subspace) { space_setup_handle_subspace_removed(space, subspace); });
}

template<typename Space>
void space_setup_clear(Space& space)
{
    space.stacking.order.lock();

    auto const windows_copy = space.windows;
    for (auto const& win : windows_copy) {
        std::visit(overload{[&](typename Space::wayland_window* win) {
                                if (!win->remnant) {
                                    destroy_window(win);
                                    remove_all(space.windows, typename Space::window_t(win));
                                }
                            },
                            [](auto&&) {}},
                   win);
    }

    singleton_interface::get_current_output_geometry = {};
    win::clear_space(space);
}

}
