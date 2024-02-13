/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

// Must be included first because of collision with Xlib defines.
#include <QtTest>

#include "types.h"
#include <como_export.h>

#include "como/base/output.h"
#include "como/script/platform.h"
#include "como/win/wayland/window.h"

#if USE_XWL
#include "como/base/wayland/xwl_platform.h"
#include "como/render/wayland/xwl_platform.h"
#include "como/win/wayland/xwl_space.h"

struct xcb_connection_t;
#else
#include "como/base/wayland/platform.h"
#include "como/render/wayland/platform.h"
#include "como/win/wayland/space.h"
#endif

#include <Wrapland/Client/xdg_shell.h>
#include <memory>

struct wl_signal;
struct wlr_input_device;
struct wlr_keyboard;

namespace Wrapland::Client
{
class SubSurface;
class Surface;
}

namespace como::detail::test
{

class client;

template<typename Base>
struct input_mod {
    using platform_t = input::wayland::platform<Base, input_mod>;
    std::unique_ptr<input::dbus::device_manager<platform_t>> dbus;
};

struct space_mod {
    std::unique_ptr<desktop::platform> desktop;
};

#if USE_XWL
struct base_mod {
    using platform_t = base::wayland::xwl_platform<base_mod>;
    using render_t = render::wayland::xwl_platform<platform_t>;
    using input_t = input::wayland::platform<platform_t, input_mod<platform_t>>;
    using space_t = win::wayland::xwl_space<platform_t, space_mod>;

    std::unique_ptr<render_t> render;
    std::unique_ptr<input_t> input;
    std::unique_ptr<space_t> space;
    std::unique_ptr<xwl::xwayland<space_t>> xwayland;
    std::unique_ptr<scripting::platform<space_t>> script;
};

using base_t = base::wayland::xwl_platform<base_mod>;
#else
struct base_mod {
    using platform_t = base::wayland::platform<base_mod>;
    using render_t = render::wayland::platform<platform_t>;
    using input_t = input::wayland::platform<platform_t, input_mod<platform_t>>;
    using space_t = win::wayland::space<platform_t, space_mod>;

    std::unique_ptr<render_t> render;
    std::unique_ptr<input_t> input;
    std::unique_ptr<space_t> space;
    std::unique_ptr<scripting::platform<space_t>> script;
};
using base_t = base::wayland::platform<base_mod>;
#endif

using space = base_t::space_t;
using wayland_window = win::wayland::window<space>;

struct COMO_EXPORT output {
    explicit output(QRect const& geometry);
    output(QRect const& geometry, double scale);

    // Geometry in logical space.
    QRect geometry;
    double scale;
};

COMO_EXPORT input::wayland::cursor<space::input_t>* cursor();

/**
 * Creates a Wayland Connection in a dedicated thread and creates various
 * client side objects which can be used to create windows.
 * @see destroy_wayland_connection
 */
COMO_EXPORT void setup_wayland_connection(global_selection globals = {});

/**
 * Destroys the Wayland Connection created with @link{setup_wayland_connection}.
 * This can be called from cleanup in order to ensure that no Wayland Connection
 * leaks into the next test method.
 * @see setup_wayland_connection
 */
COMO_EXPORT void destroy_wayland_connection();

COMO_EXPORT base::output* get_output(size_t index);
COMO_EXPORT void set_current_output(int index);

COMO_EXPORT void test_outputs_default();
COMO_EXPORT void test_outputs_geometries(std::vector<QRect> const& geometries);

COMO_EXPORT client& get_client();
COMO_EXPORT std::vector<client>& get_all_clients();

COMO_EXPORT bool wait_for_wayland_pointer();
COMO_EXPORT bool wait_for_wayland_touch();
COMO_EXPORT bool wait_for_wayland_keyboard();

COMO_EXPORT void flush_wayland_connection();
COMO_EXPORT void flush_wayland_connection(client const& clt);

COMO_EXPORT std::unique_ptr<Wrapland::Client::Surface> create_surface();
COMO_EXPORT std::unique_ptr<Wrapland::Client::Surface> create_surface(client const& clt);
COMO_EXPORT std::unique_ptr<Wrapland::Client::SubSurface>
create_subsurface(std::unique_ptr<Wrapland::Client::Surface> const& surface,
                  std::unique_ptr<Wrapland::Client::Surface> const& parentSurface);

enum class CreationSetup {
    CreateOnly,
    CreateAndConfigure, /// commit and wait for the configure event, making this surface ready to
                        /// commit buffers
};

COMO_EXPORT std::unique_ptr<Wrapland::Client::XdgShellToplevel>
create_xdg_shell_toplevel(std::unique_ptr<Wrapland::Client::Surface> const& surface,
                          CreationSetup = CreationSetup::CreateAndConfigure);
COMO_EXPORT std::unique_ptr<Wrapland::Client::XdgShellToplevel>
create_xdg_shell_toplevel(client const& clt,
                          std::unique_ptr<Wrapland::Client::Surface> const& surface,
                          CreationSetup = CreationSetup::CreateAndConfigure);

COMO_EXPORT std::unique_ptr<Wrapland::Client::XdgShellPopup>
create_xdg_shell_popup(std::unique_ptr<Wrapland::Client::Surface> const& surface,
                       std::unique_ptr<Wrapland::Client::XdgShellToplevel> const& parent_toplevel,
                       Wrapland::Client::xdg_shell_positioner_data positioner_data,
                       CreationSetup = CreationSetup::CreateAndConfigure);
COMO_EXPORT std::unique_ptr<Wrapland::Client::XdgShellPopup>
create_xdg_shell_popup(client const& clt,
                       std::unique_ptr<Wrapland::Client::Surface> const& surface,
                       std::unique_ptr<Wrapland::Client::XdgShellToplevel> const& parent_toplevel,
                       Wrapland::Client::xdg_shell_positioner_data positioner_data,
                       CreationSetup = CreationSetup::CreateAndConfigure);

/**
 * Commits the XdgShellToplevel to the given surface, and waits for the configure event from the
 * compositor
 */
COMO_EXPORT void
init_xdg_shell_toplevel(std::unique_ptr<Wrapland::Client::Surface> const& surface,
                        std::unique_ptr<Wrapland::Client::XdgShellToplevel> const& shell_toplevel);
COMO_EXPORT void
init_xdg_shell_popup(std::unique_ptr<Wrapland::Client::Surface> const& surface,
                     std::unique_ptr<Wrapland::Client::XdgShellPopup> const& popup);

/**
 * Creates a shared memory buffer of @p size in @p color and attaches it to the @p surface.
 * The @p surface gets damaged and committed, thus it's rendered.
 */
COMO_EXPORT void render(std::unique_ptr<Wrapland::Client::Surface> const& surface,
                        const QSize& size,
                        const QColor& color,
                        const QImage::Format& format = QImage::Format_ARGB32_Premultiplied);
COMO_EXPORT void render(client const& clt,
                        std::unique_ptr<Wrapland::Client::Surface> const& surface,
                        const QSize& size,
                        const QColor& color,
                        const QImage::Format& format = QImage::Format_ARGB32_Premultiplied);

/**
 * Creates a shared memory buffer using the supplied image @p img and attaches it to the @p surface
 */
COMO_EXPORT void render(std::unique_ptr<Wrapland::Client::Surface> const& surface,
                        const QImage& img);
COMO_EXPORT void render(client const& clt,
                        std::unique_ptr<Wrapland::Client::Surface> const& surface,
                        const QImage& img);

/**
 * Renders and then waits untill the new window is shown. Returns the created window.
 * If no window gets shown during @p timeout @c null is returned.
 */
COMO_EXPORT wayland_window*
render_and_wait_for_shown(std::unique_ptr<Wrapland::Client::Surface> const& surface,
                          QSize const& size,
                          QColor const& color,
                          QImage::Format const& format = QImage::Format_ARGB32_Premultiplied,
                          int timeout = 5000);
COMO_EXPORT wayland_window*
render_and_wait_for_shown(client const& clt,
                          std::unique_ptr<Wrapland::Client::Surface> const& surface,
                          QSize const& size,
                          QColor const& color,
                          QImage::Format const& format = QImage::Format_ARGB32_Premultiplied,
                          int timeout = 5000);

/**
 * Waits for the @p client to be destroyed.
 */
template<typename Window>
bool wait_for_destroyed(Window* window)
{
    QSignalSpy destroyedSpy(window->qobject.get(), &QObject::destroyed);
    if (!destroyedSpy.isValid()) {
        return false;
    }
    return destroyedSpy.wait();
}

template<typename Window, typename Variant>
Window* get_window(Variant window)
{
    auto helper_get = [](auto&& win) -> Window* {
        if (!std::holds_alternative<Window*>(win)) {
            return nullptr;
        }
        return std::get<Window*>(win);
    };

    if constexpr (requires(Variant win) { win.has_value(); }) {
        if (!window) {
            return nullptr;
        }
        return helper_get(*window);
    } else {
        return helper_get(window);
    }
}

template<typename Window>
auto get_wayland_window(Window window)
{
    if constexpr (requires(Window win) { win.has_value(); }) {
        return get_window<typename std::remove_pointer_t<
            std::variant_alternative_t<0, typename Window::value_type>>::space_t::wayland_window>(
            window);
    } else {
        return get_window<typename std::remove_pointer_t<
            std::variant_alternative_t<0, Window>>::space_t::wayland_window>(window);
    }
}

template<typename Window>
auto get_x11_window(Window window)
{
    if constexpr (requires(Window win) { win.has_value(); }) {
        return get_window<typename std::remove_pointer_t<
            std::variant_alternative_t<0, typename Window::value_type>>::space_t::x11_window>(
            window);
    } else {
        return get_window<typename std::remove_pointer_t<
            std::variant_alternative_t<0, Window>>::space_t::x11_window>(window);
    }
}

template<typename Window>
auto get_internal_window(Window window)
{
    if constexpr (requires(Window win) { win.has_value(); }) {
        return get_window<typename std::remove_pointer_t<
            std::variant_alternative_t<0, typename Window::value_type>>::space_t::
                              internal_window_t>(window);
    } else {
        return get_window<typename std::remove_pointer_t<
            std::variant_alternative_t<0, Window>>::space_t::internal_window_t>(window);
    }
}

/**
 * Locks the screen and waits till the screen is locked.
 * @returns @c true if the screen could be locked, @c false otherwise
 */
COMO_EXPORT void lock_screen();

/**
 * Unlocks the screen and waits till the screen is unlocked.
 * @returns @c true if the screen could be unlocked, @c false otherwise
 */
COMO_EXPORT void unlock_screen();

COMO_EXPORT void wlr_signal_emit_safe(wl_signal* signal, void* data);

COMO_EXPORT void pointer_motion_absolute(QPointF const& position, uint32_t time);

COMO_EXPORT void pointer_button_pressed(uint32_t button, uint32_t time);
COMO_EXPORT void pointer_button_released(uint32_t button, uint32_t time);

COMO_EXPORT void pointer_axis_horizontal(double delta, uint32_t time, int32_t discrete_delta);
COMO_EXPORT void pointer_axis_vertical(double delta, uint32_t time, int32_t discrete_delta);

COMO_EXPORT void keyboard_key_pressed(uint32_t key, uint32_t time);
COMO_EXPORT void keyboard_key_released(uint32_t key, uint32_t time);

COMO_EXPORT void keyboard_key_pressed(uint32_t key, uint32_t time, wlr_keyboard* keyboard);
COMO_EXPORT void keyboard_key_released(uint32_t key, uint32_t time, wlr_keyboard* keyboard);

COMO_EXPORT void touch_down(int32_t id, QPointF const& position, uint32_t time);
COMO_EXPORT void touch_up(int32_t id, uint32_t time);
COMO_EXPORT void touch_motion(int32_t id, QPointF const& position, uint32_t time);
COMO_EXPORT void touch_cancel();

COMO_EXPORT void swipe_begin(uint32_t fingers, uint32_t time);
COMO_EXPORT void swipe_update(uint32_t fingers, double dx, double dy, uint32_t time);
COMO_EXPORT void swipe_end(uint32_t time);
COMO_EXPORT void swipe_cancel(uint32_t time);

COMO_EXPORT void pinch_begin(uint32_t fingers, uint32_t time);
COMO_EXPORT void
pinch_update(uint32_t fingers, double dx, double dy, double scale, double rotation, uint32_t time);
COMO_EXPORT void pinch_end(uint32_t time);
COMO_EXPORT void pinch_cancel(uint32_t time);

COMO_EXPORT void hold_begin(uint32_t fingers, uint32_t time);
COMO_EXPORT void hold_end(uint32_t time);
COMO_EXPORT void hold_cancel(uint32_t time);

COMO_EXPORT void prepare_app_env(std::string const& qpa_plugin_path);
COMO_EXPORT void prepare_sys_env(std::string const& socket_name);
COMO_EXPORT std::string create_socket_name(std::string base);

#if USE_XWL
using xcb_connection_ptr = std::unique_ptr<xcb_connection_t, void (*)(xcb_connection_t*)>;

COMO_EXPORT xcb_connection_ptr xcb_connection_create();
#endif
}
