/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "texture.h"

#include "base/output.h"
#include <render/effect/interface/paint_data.h>

#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QRegion>

namespace como::render::gl
{

inline void create_view_projection(QRect const& world_geo, QMatrix4x4& view, QMatrix4x4& projection)
{
    // Can't use world_geo.center() because of QRect bottom-right offset.
    auto const view_center = QPoint(world_geo.left() + world_geo.width() / 2.,
                                    world_geo.top() + world_geo.height() / 2.);
    auto y_fov = 60.;
    auto distance = world_geo.height() / (2. * std::tan(y_fov * M_PI / 360.0f));

    view.translate(-view_center.x(), -view_center.y(), -distance);
    projection.perspective(60, world_geo.width() / (double)world_geo.height(), 0.1, distance * 2);
}

/**
 * @brief The backend creates and holds the OpenGL context and is responsible for Texture from
 * Pixmap.
 *
 * The backend is an abstract base class used by the SceneOpenGL to abstract away the
 * differences between various OpenGL windowing systems such as GLX and EGL.
 *
 * A concrete implementation has to create and release the OpenGL context in a way so that the
 * SceneOpenGL does not have to care about it.
 *
 * In addition a major task for this class is to generate the gl::texture_private which is
 * able to perform the texture from pixmap operation in the given backend.
 *
 * @author Martin Gräßlin <mgraesslin@kde.org>
 */
template<typename Scene, typename Platform>
class backend
{
public:
    using type = backend<Scene, Platform>;
    using buffer_t = typename Scene::buffer_t;
    using texture_t = texture<type>;
    using texture_priv_t = texture_private<type>;

    backend(Platform& platform)
        : platform{platform}
    {
    }

    virtual ~backend() = default;

    /**
     * @return Time passes since start of rendering current frame.
     * @see startRenderTimer
     */
    qint64 renderTime()
    {
        return m_renderTimer.nsecsElapsed();
    }
    void startRenderTimer()
    {
        m_renderTimer.start();
    }

    virtual void screenGeometryChanged(const QSize& size) = 0;
    virtual std::unique_ptr<texture_priv_t> createBackendTexture(texture_t* texture) = 0;

    virtual void endRenderingFrameForScreen(base::output* /*output*/,
                                            QRegion const& /*damage*/,
                                            QRegion const& /*damagedRegion*/)
    {
        assert(false);
    }

    virtual effect::render_data set_render_target_to_output(base::output const& /*output*/) = 0;

    virtual QRegion get_output_render_region(base::output const& /*output*/) const = 0;

    virtual void try_present()
    {
        assert(false);
    }

    virtual bool makeCurrent() = 0;
    virtual void doneCurrent() = 0;
    virtual bool hasSwapEvent() const
    {
        return true;
    }

    /**
     * @return bool Whether the scene needs to flush a frame.
     */
    bool hasPendingFlush() const
    {
        return !m_lastDamage.isEmpty();
    }

    /**
     * @brief Whether the backend uses direct rendering.
     *
     * Some OpenGLScene modes require direct rendering. E.g. the OpenGL 2 should not be used
     * if direct rendering is not supported by the Scene.
     *
     * @return bool @c true if the GL context is direct, @c false if indirect
     */
    bool isDirectRendering() const
    {
        return m_directRendering;
    }

    bool supportsBufferAge() const
    {
        return m_haveBufferAge;
    }

    bool supportsSurfacelessContext() const
    {
        return m_haveSurfacelessContext;
    }

    /**
     * Returns the damage that has accumulated since a buffer of the given age was presented.
     */
    QRegion accumulatedDamageHistory(int bufferAge) const
    {
        QRegion region;

        // Note: An age of zero means the buffer contents are undefined
        if (bufferAge > 0 && bufferAge <= m_damageHistory.count()) {
            for (int i = 0; i < bufferAge - 1; i++)
                region |= m_damageHistory[i];
        } else {
            auto const& size = platform.base.topology.size;
            region = QRegion(0, 0, size.width(), size.height());
        }

        return region;
    }

    /**
     * Saves the given region to damage history.
     */
    void addToDamageHistory(const QRegion& region)
    {
        if (m_damageHistory.count() > 10)
            m_damageHistory.removeLast();

        m_damageHistory.prepend(region);
    }

    /**
     * The backend specific extensions (e.g. EGL/GLX extensions).
     *
     * Not the OpenGL (ES) extension!
     */
    QList<QByteArray> extensions() const
    {
        return m_extensions;
    }

    /**
     * @returns whether the backend specific extensions contains @p extension.
     */
    bool hasExtension(const QByteArray& extension) const
    {
        return m_extensions.contains(extension);
    }

    /**
     * Sets the platform-specific @p extensions.
     *
     * These are the EGL/GLX extensions, not the OpenGL extensions
     */
    void setExtensions(const QList<QByteArray>& extensions)
    {
        m_extensions = extensions;
    }

    /**
     * @brief Sets whether the OpenGL context is direct.
     *
     * Should be called by the concrete subclass once it is determined whether the OpenGL context is
     * direct or indirect.
     * If the subclass does not call this method, the backend defaults to @c false.
     *
     * @param direct @c true if the OpenGL context is direct, @c false if indirect
     */
    void setIsDirectRendering(bool direct)
    {
        m_directRendering = direct;
    }

    void setSupportsSurfacelessContext(bool value)
    {
        m_haveSurfacelessContext = value;
    }

    void setSupportsBufferAge(bool value)
    {
        m_haveBufferAge = value;
    }

    /**
     * Copy a region of pixels from the current read to the current draw buffer
     */
    void copyPixels(const QRegion& region)
    {
        auto const height = platform.base.topology.size.height();
        for (const QRect& r : region) {
            const int x0 = r.x();
            const int y0 = height - r.y() - r.height();
            const int x1 = r.x() + r.width();
            const int y1 = height - r.y();

            glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    }

    Platform& platform;

protected:
    /**
     * @return const QRegion& Damage of previously rendered frame
     */
    const QRegion& lastDamage() const
    {
        return m_lastDamage;
    }
    void setLastDamage(const QRegion& damage)
    {
        m_lastDamage = damage;
    }

private:
    /**
     * @brief Whether direct rendering is used, defaults to @c false.
     */
    bool m_directRendering{false};
    /**
     * @brief Whether the backend supports GLX_EXT_buffer_age / EGL_EXT_buffer_age.
     */
    bool m_haveBufferAge{false};
    /**
     * @brief Whether the backend supports EGL_KHR_surfaceless_context.
     */
    bool m_haveSurfacelessContext = false;
    /**
     * @brief Damaged region of previously rendered frame.
     */
    QRegion m_lastDamage;
    /**
     * @brief The damage history for the past 10 frames.
     */
    QList<QRegion> m_damageHistory;
    /**
     * @brief Timer to measure how long a frame renders.
     */
    QElapsedTimer m_renderTimer;

    QList<QByteArray> m_extensions;
};

}
