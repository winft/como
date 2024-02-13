/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "output.h"

#include <como/base/backend/wlroots/backend.h>
#include <como/base/platform_qobject.h>
#include <como/base/singleton_interface.h>
#include <como/base/wayland/platform_helpers.h>
#include <como/input/wayland/platform.h>
#include <como/render/wayland/platform.h>
#include <como/win/wayland/space.h>

#include <QProcessEnvironment>
#include <Wrapland/Server/drm_lease_v1.h>
#include <cassert>
#include <memory>
#include <vector>

namespace como::base::wayland
{

template<typename Mod>
class platform;

struct platform_mod {
    using platform_t = base::wayland::platform<platform_mod>;
    using render_t = render::wayland::platform<platform_t>;
    using input_t = input::wayland::platform<platform_t>;
    using space_t = win::wayland::space<platform_t>;

    std::unique_ptr<render_t> render;
    std::unique_ptr<input_t> input;
    std::unique_ptr<space_t> space;
};

template<typename Mod = platform_mod>
class platform
{
public:
    using type = platform<Mod>;
    using qobject_t = platform_qobject;
    using backend_t = backend::wlroots::backend<type>;
    using output_t = output<type>;

    using render_t = typename Mod::render_t;
    using input_t = typename Mod::input_t;
    using space_t = typename Mod::space_t;

    platform(platform_arguments const& args)
        : qobject{std::make_unique<platform_qobject>([this] { return topology.max_scale; })}
        , operation_mode{args.mode}
        , config{args.config}
        , server{std::make_unique<wayland::server<type>>(*this, args.socket_name, args.flags)}
        , backend{*this, args.headless}
    {
        wayland::platform_init(*this);
    }

    platform(type const&) = delete;
    platform& operator=(type const&) = delete;
    platform(type&& other) = delete;
    platform& operator=(type&& other) = delete;

    virtual ~platform()
    {
        platform_cleanup(*this);
        singleton_interface::get_outputs = {};
    }

    std::unique_ptr<platform_qobject> qobject;
    base::operation_mode operation_mode;
    output_topology topology;
    base::config config;
    std::unique_ptr<base::options> options;

    std::unique_ptr<wayland::server<type>> server;
    std::unique_ptr<Wrapland::Server::drm_lease_device_v1> drm_lease_device;

    // All outputs, including disabled ones.
    std::vector<output_t*> all_outputs;

    // Enabled outputs only, so outputs that are relevant for our compositing.
    std::vector<output_t*> outputs;

    std::unique_ptr<base::seat::session> session;
    backend_t backend;
    QProcessEnvironment process_environment;

    Mod mod;
};

}
