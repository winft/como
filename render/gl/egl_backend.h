/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#pragma once

#include "backend.h"
#include "texture.h"

#include <QObject>
#include <epoxy/egl.h>

class QOpenGLFramebufferObject;

namespace Wrapland
{
namespace Server
{
class Buffer;
class ShmImage;
}
}

namespace KWin::render::gl
{

class egl_dmabuf;

class KWIN_EXPORT egl_backend : public QObject, public backend
{
    Q_OBJECT
public:
    ~egl_backend() override;
    bool makeCurrent() override;
    void doneCurrent() override;
    render::gl::texture_private* createBackendTexture(render::gl::texture* texture) override;

    EGLDisplay eglDisplay() const
    {
        return m_display;
    }
    EGLContext context() const
    {
        return m_context;
    }
    EGLSurface surface() const
    {
        return m_surface;
    }
    EGLConfig config() const
    {
        return m_config;
    }

    bool hasClientExtension(const QByteArray& ext) const;
    bool isOpenGLES() const;
    void setConfig(const EGLConfig& config);
    void setSurface(const EGLSurface& surface);

    egl_dmabuf* dmabuf{nullptr};

protected:
    egl_backend();
    void setEglDisplay(const EGLDisplay& display);
    void cleanup();
    virtual void cleanupSurfaces();
    bool initEglAPI();
    void initKWinGL();
    void initBufferAge();
    void initClientExtensions();

    bool createContext();

private:
    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLConfig m_config = nullptr;
    QList<QByteArray> m_clientExtensions;
};

class KWIN_EXPORT egl_texture : public render::gl::texture_private
{
public:
    egl_texture(render::gl::texture* texture, egl_backend* backend);
    ~egl_texture() override;
    bool loadTexture(render::window_pixmap* pixmap) override;
    void updateTexture(render::window_pixmap* pixmap) override;
    render::gl::backend* backend() override;

protected:
    EGLImageKHR image() const
    {
        return m_image;
    }
    void setImage(const EGLImageKHR& img)
    {
        m_image = img;
    }
    render::gl::texture* texture() const
    {
        return q;
    }

private:
    void textureSubImage(int scale, Wrapland::Server::ShmImage const& img, const QRegion& damage);
    void textureSubImageFromQImage(int scale, const QImage& image, const QRegion& damage);

    bool createTextureImage(const QImage& image);
    bool loadShmTexture(Wrapland::Server::Buffer* buffer);
    bool loadEglTexture(Wrapland::Server::Buffer* buffer);
    bool loadDmabufTexture(Wrapland::Server::Buffer* buffer);
    bool loadInternalImageObject(render::window_pixmap* pixmap);
    EGLImageKHR attach(Wrapland::Server::Buffer* buffer);
    bool updateFromFBO(const QSharedPointer<QOpenGLFramebufferObject>& fbo);
    bool updateFromInternalImageObject(render::window_pixmap* pixmap);
    render::gl::texture* q;
    egl_backend* m_backend;
    EGLImageKHR m_image;
    bool m_hasSubImageUnpack{false};
};

}
