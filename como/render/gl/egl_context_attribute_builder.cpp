/*
SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "egl_context_attribute_builder.h"

#include <epoxy/egl.h>

namespace como::render::gl
{

std::vector<int> egl_context_attribute_builder::build() const
{
    std::vector<int> attribs;
    if (isVersionRequested()) {
        attribs.emplace_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
        attribs.emplace_back(majorVersion());
        attribs.emplace_back(EGL_CONTEXT_MINOR_VERSION_KHR);
        attribs.emplace_back(minorVersion());
    }
    int contextFlags = 0;
    if (isRobust()) {
        attribs.emplace_back(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR);
        attribs.emplace_back(EGL_LOSE_CONTEXT_ON_RESET_KHR);
        contextFlags |= EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR;
    }
    if (isForwardCompatible()) {
        contextFlags |= EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR;
    }
    if (contextFlags != 0) {
        attribs.emplace_back(EGL_CONTEXT_FLAGS_KHR);
        attribs.emplace_back(contextFlags);
    }
    if (isCoreProfile() || isCompatibilityProfile()) {
        attribs.emplace_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
        if (isCoreProfile()) {
            attribs.emplace_back(EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR);
        } else if (isCompatibilityProfile()) {
            attribs.emplace_back(EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR);
        }
    }
    if (isHighPriority()) {
        attribs.emplace_back(EGL_CONTEXT_PRIORITY_LEVEL_IMG);
        attribs.emplace_back(EGL_CONTEXT_PRIORITY_HIGH_IMG);
    }
    attribs.emplace_back(EGL_NONE);
    return attribs;
}

std::vector<int> egl_gles_context_attribute_builder::build() const
{
    std::vector<int> attribs;
    attribs.emplace_back(EGL_CONTEXT_CLIENT_VERSION);
    attribs.emplace_back(majorVersion());
    if (isRobust()) {
        attribs.emplace_back(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT);
        attribs.emplace_back(EGL_TRUE);
        attribs.emplace_back(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT);
        attribs.emplace_back(EGL_LOSE_CONTEXT_ON_RESET_EXT);
    }
    if (isHighPriority()) {
        attribs.emplace_back(EGL_CONTEXT_PRIORITY_LEVEL_IMG);
        attribs.emplace_back(EGL_CONTEXT_PRIORITY_HIGH_IMG);
    }
    attribs.emplace_back(EGL_NONE);
    return attribs;
}

}
