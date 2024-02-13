/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "singleton_interface.h"

namespace como::render
{

render::compositor_qobject* singleton_interface::compositor{nullptr};
EffectsHandler* singleton_interface::effects{nullptr};
std::function<bool()> singleton_interface::supports_surfaceless_context{};
std::function<gl::egl_data*()> singleton_interface::get_egl_data{};

}
