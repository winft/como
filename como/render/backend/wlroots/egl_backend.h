/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <config-como.h>

#include "egl_helpers.h"
#include "egl_output.h"
#include "egl_texture.h"
#include "wlr_helpers.h"

#include <como/render/gl/backend.h>
#include <como/render/gl/egl.h>
#include <como/render/gl/gl.h>
#include <como/render/wayland/egl.h>
#include <como/render/wayland/egl_data.h>

#include <como/render/gl/interface/platform.h>
#include <como/render/gl/interface/utils.h>

#include <QOpenGLContext>
#include <Wrapland/Server/linux_dmabuf_v1.h>
#include <memory>
#include <stdexcept>

namespace como::render::backend::wlroots
{

template<typename Backend>
class egl_backend : public gl::backend<gl::scene<typename Backend::frontend_type>,
                                       typename Backend::frontend_type>
{
public:
    using type = egl_backend<Backend>;
    using gl_scene = gl::scene<typename Backend::frontend_type>;
    using abstract_type = gl::backend<gl_scene, typename Backend::frontend_type>;
    using egl_output_t = egl_output<typename Backend::output_t>;

    egl_backend(Backend& backend)
        : abstract_type(*backend.frontend)
        , backend{backend}
    {
        native = wlr_gles2_renderer_get_egl(backend.renderer);

        data.base.display = wlr_egl_get_display(native);
        data.base.context = wlr_egl_get_context(native);

        load_egl_proc(&data.base.create_image_khr, "eglCreateImageKHR");
        load_egl_proc(&data.base.destroy_image_khr, "eglDestroyImageKHR");

        backend.frontend->egl_data = &data.base;

        // Egl is always direct rendering.
        this->setIsDirectRendering(true);

        gl::init_client_extensions(*this);
        gl::init_server_extensions(*this);

        for (auto& out : backend.frontend->base.all_outputs) {
            auto render = static_cast<typename Backend::output_t*>(out->render.get());
            get_egl_out(out) = std::make_unique<egl_output_t>(*render, data);
        }

        make_context_current(data);

        gl::init_gl(gl_interface::egl, get_proc_address);
        gl::init_buffer_age(*this);
        wayland::init_egl(*this, data);

        if (this->hasExtension(QByteArrayLiteral("EGL_EXT_image_dma_buf_import"))) {
#if WLR_HAVE_NEW_PIXEL_COPY_API
            auto const formats_set
                = wlr_renderer_get_texture_formats(backend.renderer, WLR_BUFFER_CAP_DMABUF);
#else
            auto const formats_set = wlr_renderer_get_dmabuf_texture_formats(backend.renderer);
#endif
            auto const formats_map = get_drm_formats<Wrapland::Server::drm_format>(formats_set);

            dmabuf = std::make_unique<Wrapland::Server::linux_dmabuf_v1>(
                backend.frontend->base.server->display.get(),
                [](auto const& planes, auto format, auto modifier, auto const& size, auto flags) {
                    return std::make_unique<Wrapland::Server::linux_dmabuf_buffer_v1>(
                        planes, format, modifier, size, flags);
                });
            dmabuf->set_formats(formats_map);
        }
    }

    ~egl_backend() override
    {
        tear_down();
    }

    void tear_down()
    {
        if (!backend.frontend->egl_data) {
            // Already cleaned up.
            return;
        }

        cleanup();

        backend.frontend->egl_data = nullptr;
        data = {};
    }

    // TODO(romangg): Is there a reasonable difference between a plain eglMakeCurrent call that this
    // function does and the override, where we set doneCurrent on the QOpenGLContext? Otherwise we
    // could merge the calls.
    void make_current()
    {
        make_context_current(data);
    }

    bool makeCurrent() override
    {
        if (auto context = QOpenGLContext::currentContext()) {
            // Workaround to tell Qt that no QOpenGLContext is current
            context->doneCurrent();
        }
        make_context_current(data);
        return is_context_current(data);
    }

    void doneCurrent() override
    {
        unset_context_current(data);
    }

    void screenGeometryChanged(QSize const& /*size*/) override
    {
        // TODO, create new buffer?
    }

    std::unique_ptr<typename abstract_type::texture_priv_t>
    createBackendTexture(typename abstract_type::texture_t* texture) override
    {
        return std::make_unique<egl_texture<type>>(texture, this);
    }

    effect::render_data set_render_target_to_output(base::output const& output) override
    {
        auto const& out = get_egl_out(&output);
        auto viewport = out->out->base.view_geometry();
        auto res = out->out->base.mode_size();
        auto is_portrait = has_portrait_transform(out->out->base);

        if (is_portrait) {
            // The wlroots buffer is always sideways.
            viewport = viewport.transposed();
        }

        auto const& output_impl = static_cast<typename Backend::output_t::base_t const&>(output);
        auto native_out = output_impl.native;

#if WLR_HAVE_NEW_PIXEL_COPY_API
        const_cast<typename Backend::output_t::base_t&>(output_impl).ensure_next_state();

        assert(!current_render_pass);
        current_render_pass = wlr_output_begin_render_pass(
            native_out, output_impl.next_state->get_native(), &out->bufferAge, nullptr);
#else
        wlr_output_attach_render(native_out, &out->bufferAge);
        wlr_renderer_begin(backend.renderer, viewport.width(), viewport.height());
#endif

        auto transform = static_cast<effect::transform_type>(get_transform(output_impl));

        QMatrix4x4 view;
        QMatrix4x4 projection;
        gl::create_view_projection(output.geometry(), view, projection);

        effect::render_data data{
            .targets = render_targets,
            .view = view,
            .projection = effect::get_transform_matrix(transform) * projection,
            .viewport = viewport,
            .transform = transform,
            .flip_y = true,
        };

#if WLR_HAVE_NEW_PIXEL_COPY_API
        native_fbo
            = GLFramebuffer(wlr_gles2_renderer_get_buffer_fbo(
                                backend.renderer, output_impl.next_state->get_native()->buffer),
                            res,
                            viewport);
#else
        native_fbo
            = GLFramebuffer(wlr_gles2_renderer_get_current_fbo(backend.renderer), res, viewport);
#endif
        push_framebuffer(data, &native_fbo);

        return data;
    }

    QRegion get_output_render_region(base::output const& output) const override
    {
        auto const& out = get_egl_out(&output);

        if (!this->supportsBufferAge()) {
            // If buffer age exenstion is not supported we always repaint the whole output as we
            // don't know the status of the back buffer we render to.
            return output.geometry();
        }
        if (out->bufferAge == 0) {
            // If buffer age is 0, the contents of the back buffer we now will render to are
            // undefined and it has to be repainted completely.
            return output.geometry();
        }
        if (out->bufferAge > static_cast<int>(out->damageHistory.size())) {
            // If buffer age is older than our damage history has recorded we do not have all damage
            // logged for that age and we need to repaint completely.
            return output.geometry();
        }

        // But if all conditions are satisfied we can look up our damage history up until to the
        // buffer age and repaint only that.
        QRegion region;
        for (int i = 0; i < out->bufferAge - 1; i++) {
            region |= out->damageHistory[i];
        }
        return region;
    }

    void endRenderingFrameForScreen(base::output* output,
                                    QRegion const& renderedRegion,
                                    QRegion const& damagedRegion) override
    {
        auto& out = get_egl_out(output);
        auto impl_out = static_cast<typename Backend::output_t::base_t*>(output);

        if (GLPlatform::instance()->supports(GLFeature::TimerQuery)) {
            out->out->last_timer_queries.emplace_back();
        }

        render_targets.pop();
        assert(render_targets.empty());

#if WLR_HAVE_NEW_PIXEL_COPY_API
        assert(current_render_pass);
        wlr_render_pass_submit(current_render_pass);
        current_render_pass = nullptr;
#else
        wlr_renderer_end(backend.renderer);
#endif

        if (damagedRegion.intersected(output->geometry()).isEmpty()) {
            // If the damaged region of a window is fully occluded, the only
            // rendering done, if any, will have been to repair a reused back
            // buffer, making it identical to the front buffer.
            //
            // In this case we won't post the back buffer. Instead we'll just
            // set the buffer age to 1, so the repaired regions won't be
            // rendered again in the next frame.
            if (!renderedRegion.intersected(output->geometry()).isEmpty()) {
                glFlush();
            }

#if !WLR_HAVE_NEW_PIXEL_COPY_API
            wlr_output_rollback(impl_out->native);
#endif
            return;
        }

        set_output_damage(impl_out, damagedRegion.translated(-output->geometry().topLeft()));

        if (!out->present()) {
            out->out->swap_pending = false;
            return;
        }

        if (this->supportsBufferAge()) {
            if (out->damageHistory.size() > 10) {
                out->damageHistory.pop_back();
            }
            out->damageHistory.push_front(damagedRegion.intersected(output->geometry()));
        }
    }

    bool hasClientExtension(const QByteArray& ext) const
    {
        return data.base.client_extensions.contains(ext);
    }

    std::unique_ptr<egl_output<typename Backend::output_t>>&
    get_egl_out(base::output const* out) const
    {
        using out_t = typename Backend::output_t;
        using base_wlout_t = out_t::base_t::abstract_type;
        return static_cast<out_t*>(static_cast<base_wlout_t const*>(out)->render.get())->egl;
    }

    Backend& backend;

    std::unique_ptr<Wrapland::Server::linux_dmabuf_v1> dmabuf;
    wayland::egl_data data;

    std::stack<framebuffer*> render_targets;
    GLFramebuffer native_fbo;
    wlr_egl* native{nullptr};

private:
    void cleanup()
    {
        cleanupGL();
        doneCurrent();
        cleanupSurfaces();

        dmabuf.reset();
    }

    void cleanupSurfaces()
    {
        for (auto out : backend.frontend->base.all_outputs) {
            get_egl_out(out).reset();
        }
    }

    template<typename Output>
    void set_output_damage(Output* output, QRegion const& src_damage) const
    {
        auto damage = create_pixman_region(src_damage);

        int width, height;
        wlr_output_transformed_resolution(output->native, &width, &height);

        enum wl_output_transform transform = wlr_output_transform_invert(output->native->transform);
        wlr_region_transform(&damage, &damage, transform, width, height);

#if WLR_HAVE_NEW_PIXEL_COPY_API
        wlr_output_state_set_damage(output->next_state->get_native(), &damage);
#else
        wlr_output_set_damage(output->native, &damage);
#endif
        pixman_region32_fini(&damage);
    }

#if WLR_HAVE_NEW_PIXEL_COPY_API
    wlr_render_pass* current_render_pass{nullptr};
#endif
};

}
