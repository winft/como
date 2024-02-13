/*
    SPDX-FileCopyrightText: 2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como_export.h>

#include <epoxy/gl.h>
#include <functional>

// qopengl.h declares GLdouble as a typedef of float when Qt is built
// with GLES support.  This conflicts with the epoxy/gl_generated.h
// declaration, so we have to prevent the Qt header from being #included.
#define QOPENGL_H

#ifndef QOPENGLF_APIENTRY
#define QOPENGLF_APIENTRY GLAPIENTRY
#endif

#ifndef QOPENGLF_APIENTRYP
#define QOPENGLF_APIENTRYP GLAPIENTRYP
#endif

namespace como
{

typedef void (*resolveFuncPtr)();
void COMO_EXPORT
glResolveFunctions(const std::function<resolveFuncPtr(const char*)>& resolveFunction);

// GL_ARB_robustness / GL_EXT_robustness
using glGetGraphicsResetStatus_func = GLenum (*)();
using glReadnPixels_func = void (*)(GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    GLsizei bufSize,
                                    GLvoid* data);
using glGetnUniformfv_func
    = void (*)(GLuint program, GLint location, GLsizei bufSize, GLfloat* params);

extern COMO_EXPORT glGetGraphicsResetStatus_func glGetGraphicsResetStatus;
extern COMO_EXPORT glReadnPixels_func glReadnPixels;
extern COMO_EXPORT glGetnUniformfv_func glGetnUniformfv;

}
