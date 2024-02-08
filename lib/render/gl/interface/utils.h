/*
    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como_export.h>

#include <QByteArray>
#include <QList>
#include <epoxy/gl.h>

namespace como
{

// Initializes OpenGL stuff. This includes resolving function pointers as
//  well as checking for GL version and extensions
//  Note that GL context has to be created by the time this function is called
typedef void (*resolveFuncPtr)();
void COMO_EXPORT initGL(const std::function<resolveFuncPtr(const char*)>& resolveFunction);
// Cleans up all resources hold by the GL Context
void COMO_EXPORT cleanupGL();

bool COMO_EXPORT hasGLVersion(int major, int minor, int release = 0);
// use for both OpenGL and GLX extensions
bool COMO_EXPORT hasGLExtension(const QByteArray& extension);

QString COMO_EXPORT formatGLError(GLenum err);

// detect OpenGL error (add to various places in code to pinpoint the place)
bool COMO_EXPORT checkGLError(const char* txt);

QList<QByteArray> COMO_EXPORT openGLExtensions();

}
