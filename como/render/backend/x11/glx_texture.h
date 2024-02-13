/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "render/gl/backend.h"
#include "render/gl/texture.h"
#include "render/window.h"
#include "render/x11/buffer.h"
#include "win/geo.h"

// Must be included late because of Qt.
#include "glx_data.h"
#include "glx_fb_config.h"

#include <epoxy/glx.h>
#include <xcb/glx.h>

namespace como::render::backend::x11
{

/// Texture using an GLXPixmap.
template<typename Backend>
class GlxTexture : public gl::texture_private<typename Backend::abstract_type>
{
public:
    using buffer_t = typename Backend::buffer_t;

    GlxTexture(gl::texture<typename Backend::abstract_type>* texture, Backend* backend)
        : gl::texture_private<typename Backend::abstract_type>()
        , q(texture)
        , m_backend(backend)
        , m_glxpixmap(None)
    {
    }

    ~GlxTexture() override
    {
        if (m_glxpixmap != None) {
            if (!m_backend->platform.options->qobject->isGlStrictBinding()) {
                glXReleaseTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT);
            }
            glXDestroyPixmap(display(), m_glxpixmap);
            m_glxpixmap = None;
        }
    }

    void onDamage() override
    {
        if (m_backend->platform.options->qobject->isGlStrictBinding() && m_glxpixmap) {
            glXReleaseTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT);
            glXBindTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT, nullptr);
        }
        GLTexturePrivate::onDamage();
    }

    bool updateTexture(buffer_t* buffer) override
    {
        if (this->m_target) {
            return true;
        }

        auto ref_win = buffer->window->ref_win;
        using var_win = std::decay_t<decltype(*ref_win)>;
        using x11_window_t = typename std::remove_pointer_t<
            std::variant_alternative_t<0, var_win>>::space_t::x11_window;

        auto x11_ref_win = std::get<x11_window_t*>(*ref_win);
        auto const size = win::render_geometry(x11_ref_win).size();
        auto const visual = x11_ref_win->xcb_visual;

        auto const& win_integrate
            = static_cast<render::x11::buffer_win_integration<typename buffer_t::abstract_type>&>(
                *buffer->win_integration);
        if (win_integrate.pixmap == XCB_NONE || size.isEmpty() || visual == XCB_NONE) {
            return false;
        }

        auto const info = fb_config_info_for_visual(visual, *m_backend);
        if (!info || info->fbconfig == nullptr)
            return false;

        if (info->texture_targets & GLX_TEXTURE_2D_BIT_EXT) {
            this->m_target = GL_TEXTURE_2D;
            this->m_scale.setWidth(1.0f / this->m_size.width());
            this->m_scale.setHeight(1.0f / this->m_size.height());
        } else {
            Q_ASSERT(info->texture_targets & GLX_TEXTURE_RECTANGLE_BIT_EXT);

            this->m_target = GL_TEXTURE_RECTANGLE;
            this->m_scale.setWidth(1.0f);
            this->m_scale.setHeight(1.0f);
        }

        const int attrs[]
            = {GLX_TEXTURE_FORMAT_EXT,
               info->bind_texture_format,
               GLX_MIPMAP_TEXTURE_EXT,
               false,
               GLX_TEXTURE_TARGET_EXT,
               this->m_target == GL_TEXTURE_2D ? GLX_TEXTURE_2D_EXT : GLX_TEXTURE_RECTANGLE_EXT,
               0};

        m_glxpixmap = glXCreatePixmap(display(), info->fbconfig, win_integrate.pixmap, attrs);
        this->m_size = size;
        q->set_content_transform(info->y_inverted ? effect::transform_type::flipped_180
                                                  : effect::transform_type::normal);

        this->m_canUseMipmaps = false;

        glGenTextures(1, &this->m_texture);

        q->setDirty();
        q->setFilter(GL_NEAREST);

        glBindTexture(this->m_target, this->m_texture);
        glXBindTexImageEXT(display(), m_glxpixmap, GLX_FRONT_LEFT_EXT, nullptr);

        this->updateMatrix();
        return true;
    }

    Backend* backend() override
    {
        return m_backend;
    }

private:
    Display* display() const
    {
        return m_backend->data.display;
    }

    gl::texture<typename Backend::abstract_type>* q;
    Backend* m_backend;

    // the glx pixmap the texture is bound to
    GLXPixmap m_glxpixmap;
};

}
