/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/render/gl/interface/platform.h>
#include <como/render/gl/interface/utils.h>

#include <epoxy/egl.h>

namespace como::render::gl
{

template<typename ProcAddrGetter>
void init_gl(gl_interface interface, ProcAddrGetter* getter)
{
    auto glPlatform = GLPlatform::create();
    glPlatform->detect(interface);
    glPlatform->printResults();
    initGL(getter);
}

}
