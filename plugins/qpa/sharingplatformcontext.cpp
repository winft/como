/*
SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "sharingplatformcontext.h"

#include "offscreensurface.h"
#include "window.h"

#include "render/gl/egl_data.h"
#include "render/singleton_interface.h"

#include <logging.h>

#include <QOpenGLFramebufferObject>
#include <private/qopenglcontext_p.h>

namespace como
{

namespace QPA
{

SharingPlatformContext::SharingPlatformContext(QOpenGLContext* context)
    : AbstractPlatformContext(context,
                              render::singleton_interface::get_egl_data()->display,
                              nullptr)
{
    create();
}

bool SharingPlatformContext::makeCurrent(QPlatformSurface* surface)
{
    bool const ok = eglMakeCurrent(eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext());
    if (!ok) {
        qCWarning(KWIN_QPA, "eglMakeCurrent failed: %x", eglGetError());
        return false;
    }

    if (surface->surface()->surfaceClass() == QSurface::Window) {
        // QOpenGLContextPrivate::setCurrentContext will be called after this
        // method returns, but that's too late, as we need a current context in
        // order to bind the content framebuffer object.
        QOpenGLContextPrivate::setCurrentContext(context());

        Window* window = static_cast<Window*>(surface);
        window->bindContentFBO();
    }

    return true;
}

bool SharingPlatformContext::isSharing() const
{
    return false;
}

void SharingPlatformContext::swapBuffers(QPlatformSurface* surface)
{
    if (surface->surface()->surfaceClass() == QSurface::Window) {
        Window* window = static_cast<Window*>(surface);
        auto* client = window->client();
        if (!client) {
            return;
        }
        glFlush();
        auto fbo = window->swapFBO();
        window->bindContentFBO();
        client->present_fbo(fbo);
    }
}

GLuint SharingPlatformContext::defaultFramebufferObject(QPlatformSurface* surface) const
{
    if (Window* window = dynamic_cast<Window*>(surface)) {
        const auto& fbo = window->contentFBO();
        if (fbo) {
            return fbo->handle();
        }
        qCDebug(KWIN_QPA) << "No default framebuffer object for internal window";
    }

    return 0;
}

void SharingPlatformContext::create()
{
    if (!config()) {
        qCWarning(KWIN_QPA) << "Did not get an EGL config";
        return;
    }
    if (!bindApi()) {
        qCWarning(KWIN_QPA) << "Could not bind API.";
        return;
    }
    createContext(render::singleton_interface::get_egl_data()->context);
}

}
}
