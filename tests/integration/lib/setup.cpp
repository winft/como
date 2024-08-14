/*´
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "setup.h"

#include "como/base/backend/wlroots/helpers.h"
#include "como/base/config.h"
#include "como/desktop/kde/platform.h"
#include "como/desktop/kde/screen_locker.h"
#include "como/input/wayland/platform.h"
#include "como/render/shortcuts_init.h"
#include "como/render/wayland/platform.h"
#include "como/render/wayland/xwl_platform.h"
#include "como/win/shortcuts_init.h"

extern "C" {
#include <wlr/backend/headless.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/interfaces/wlr_touch.h>
}

#include <iostream>

namespace como::detail::test
{

static setup* current_setup{nullptr};

setup::setup(std::string const& test_name)
    : setup(test_name, base::operation_mode::wayland, base::wayland::start_options::none)
{
}

setup::setup(std::string const& test_name, base::operation_mode mode)
    : setup(test_name, mode, base::wayland::start_options::lock_screen_integration)
{
}

setup::setup(std::string const& test_name,
             base::operation_mode mode,
             base::wayland::start_options flags)
{
    current_setup = this;

    auto const socket_name = create_socket_name(test_name);

    auto rm_config = [](auto name) {
        auto const path = QStandardPaths::locate(QStandardPaths::ConfigLocation, name);
        if (!path.isEmpty()) {
            QFile{path}.remove();
        }
    };

    rm_config("kcminputrc");
    rm_config("kxkbrc");
    rm_config("kglobalshortcutsrc");

    QIcon::setThemeName(QStringLiteral("breeze"));

    qunsetenv("XKB_DEFAULT_RULES");
    qunsetenv("XKB_DEFAULT_MODEL");
    qunsetenv("XKB_DEFAULT_LAYOUT");
    qunsetenv("XKB_DEFAULT_VARIANT");
    qunsetenv("XKB_DEFAULT_OPTIONS");

    auto breezerc = KSharedConfig::openConfig(QStringLiteral("breezerc"));
    breezerc->group(QStringLiteral("Common"))
        .writeEntry(QStringLiteral("OutlineIntensity"), QStringLiteral("OutlineOff"));
    breezerc->sync();

    base = std::make_unique<base_t>(base::wayland::platform_arguments{
        .config = base::config(KConfig::OpenFlag::SimpleConfig, ""),
        .socket_name = socket_name,
        .flags = flags,
        .mode = mode,
        .headless = true,
    });

    auto headless_backend = base::backend::wlroots::get_headless_backend(base->backend.native);
    auto out = wlr_headless_add_output(headless_backend, 1280, 1024);

    wlr_output_state wlr_out_state;
    wlr_output_state_init(&wlr_out_state);
    wlr_output_state_set_enabled(&wlr_out_state, true);
    wlr_output_commit_state(out, &wlr_out_state);

    try {
        base->mod.render = std::make_unique<base_t::render_t>(*base);
    } catch (std::system_error const& exc) {
        std::cerr << "FATAL ERROR: render creation failed: " << exc.what() << std::endl;
        throw;
    }

    base->process_environment.insert(QStringLiteral("WAYLAND_DISPLAY"), socket_name.c_str());
    prepare_sys_env(socket_name);
}

setup::~setup()
{
    current_setup = nullptr;
}

void setup::start()
{
    base->options = base::create_options(base->operation_mode, base->config.main);
    base->mod.input
        = std::make_unique<base_t::input_t>(*base, input::config(KConfig::SimpleConfig));
    base->mod.input->mod.dbus
        = std::make_unique<input::dbus::device_manager<base_t::input_t>>(*base->mod.input);

    keyboard = static_cast<wlr_keyboard*>(calloc(1, sizeof(wlr_keyboard)));
    pointer = static_cast<wlr_pointer*>(calloc(1, sizeof(wlr_pointer)));
    touch = static_cast<wlr_touch*>(calloc(1, sizeof(wlr_touch)));
    assert(keyboard);
    assert(pointer);
    assert(touch);
    wlr_keyboard_init(keyboard, nullptr, "headless-keyboard");
    wlr_pointer_init(pointer, nullptr, "headless-pointer");
    wlr_touch_init(touch, nullptr, "headless-touch");

    wlr_signal_emit_safe(&base->backend.native->events.new_input, keyboard);
    wlr_signal_emit_safe(&base->backend.native->events.new_input, pointer);
    wlr_signal_emit_safe(&base->backend.native->events.new_input, touch);

    base->mod.space = std::make_unique<base_t::space_t>(*base->mod.render, *base->mod.input);
    base->mod.space->mod.desktop
        = std::make_unique<desktop::kde::platform<base_t::space_t>>(*base->mod.space);
    win::init_shortcuts(*base->mod.space);
    render::init_shortcuts(*base->mod.render);
    base->mod.script = std::make_unique<scripting::platform<base_t::space_t>>(*base->mod.space);

    base::wayland::platform_start(*base);

    // Must set physical size for calculation of screen edges corner offset.
    // TODO(romangg): Make the corner offset calculation not depend on that.
    auto out = base->outputs.at(0);
    auto metadata = out->wrapland_output()->get_metadata();
    metadata.physical_size = {1280, 1024};
    out->wrapland_output()->set_metadata(metadata);

    base->mod.space->mod.desktop->screen_locker = std::make_unique<desktop::kde::screen_locker>(
        *base->server, base->process_environment, false);

#if USE_XWL
    if (base->operation_mode == base::operation_mode::xwayland) {
        try {
            base->mod.xwayland = std::make_unique<xwl::xwayland<base_t::space_t>>(*base->mod.space);
        } catch (std::system_error const& exc) {
            std::cerr << "System error creating Xwayland: " << exc.what() << std::endl;
        } catch (std::exception const& exc) {
            std::cerr << "Exception creating Xwayland: " << exc.what() << std::endl;
        }
    }
#endif
}

void setup::set_outputs(size_t count)
{
    auto outputs = std::vector<output>();
    auto const size = QSize(1280, 1024);
    auto width = 0;

    for (size_t i = 0; i < count; i++) {
        auto const out = output({QPoint(width, 0), size});
        outputs.push_back(out);
        width += size.width();
    }

    set_outputs(outputs);
}

void setup::set_outputs(std::vector<QRect> const& geometries)
{
    auto outputs = std::vector<output>();
    for (auto&& geo : geometries) {
        auto const out = output(geo);
        outputs.push_back(out);
    }
    set_outputs(outputs);
}

void setup::set_outputs(std::vector<output> const& outputs)
{
    auto outputs_copy = base->all_outputs;
    for (auto output : outputs_copy) {
        delete output;
    }

    for (auto&& output : outputs) {
        auto const size = output.geometry.size() * output.scale;

        auto out = wlr_headless_add_output(base->backend.native, size.width(), size.height());

        wlr_output_state wlr_out_state;
        wlr_output_state_init(&wlr_out_state);
        wlr_output_state_set_enabled(&wlr_out_state, true);
        wlr_output_commit_state(out, &wlr_out_state);

        base->all_outputs.back()->force_geometry(output.geometry);
    }

    base::update_output_topology(*base);
}

void setup::add_client(global_selection globals)
{
    clients.emplace_back(globals);
}

setup* app()
{
    return current_setup;
}

}
