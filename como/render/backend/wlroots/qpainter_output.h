/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "wlr_includes.h"

#include <como/base/logging.h>

#include <QImage>
#include <memory>

struct wlr_renderer;

namespace como::render::backend::wlroots
{

template<typename Output>
class qpainter_output
{
public:
    qpainter_output(Output& output, wlr_renderer* renderer)
        : output{output}
        , renderer{renderer}
    {
    }

    qpainter_output(qpainter_output const&) = delete;
    qpainter_output& operator=(qpainter_output const&) = delete;
    qpainter_output(qpainter_output&& other) noexcept = default;
    qpainter_output& operator=(qpainter_output&& other) noexcept = default;

    void begin_render()
    {
        auto& output_base_impl = static_cast<typename Output::base_t&>(output.base);
        auto native_out = output_base_impl.native;
        auto const size = output.base.geometry().size();

#if WLR_HAVE_NEW_PIXEL_COPY_API
        output_base_impl.ensure_next_state();

        assert(!current_render_pass);
        current_render_pass = wlr_output_begin_render_pass(
            native_out, output_base_impl.next_state->get_native(), nullptr, nullptr);
#else
        wlr_output_attach_render(native_out, nullptr);
        wlr_renderer_begin(renderer, size.width(), size.height());
#endif

        if (!buffer || size != buffer->size()) {
#if WLR_HAVE_NEW_PIXEL_COPY_API
            auto img = wlr_pixman_renderer_get_buffer_image(
                renderer, output_base_impl.next_state->get_native()->buffer);
#else
            auto img = wlr_pixman_renderer_get_current_image(renderer);
#endif
            auto pixman_format = pixman_image_get_format(img);
            buffer = std::make_unique<QImage>(size, pixman_to_qt_image_format(pixman_format));
            if (buffer->isNull()) {
                // TODO(romangg): handle oom
                buffer.reset();
                return;
            }
            buffer->fill(Qt::gray);
        }
    }

    void present(QRegion const& /*damage*/)
    {
        auto& base = static_cast<typename Output::base_t&>(output.base);
        auto buffer_bits = buffer->constBits();

#if WLR_HAVE_NEW_PIXEL_COPY_API
        auto pixman_data = pixman_image_get_data(
            wlr_pixman_renderer_get_buffer_image(renderer, base.next_state->get_native()->buffer));
#else
        auto pixman_data = pixman_image_get_data(wlr_pixman_renderer_get_current_image(renderer));
#endif

        memcpy(pixman_data, buffer_bits, buffer->width() * buffer->height() * 4);

#if WLR_HAVE_NEW_PIXEL_COPY_API
        assert(current_render_pass);
        wlr_render_pass_submit(current_render_pass);
        current_render_pass = nullptr;
#endif

        output.swap_pending = true;

#if WLR_HAVE_NEW_PIXEL_COPY_API
        wlr_output_state_set_enabled(base.next_state->get_native(), true);

        if (!wlr_output_test_state(base.native, base.next_state->get_native())) {
            qCWarning(KWIN_CORE) << "Atomic output test failed on present.";
            base.next_state.reset();
            return;
        }
        if (!wlr_output_commit_state(base.native, base.next_state->get_native())) {
            qCWarning(KWIN_CORE) << "Atomic output commit failed on present.";
        }
        base.next_state.reset();
#else
        if (!base.native->enabled) {
            wlr_output_enable(base.native, true);
        }

        if (!wlr_output_test(base.native)) {
            qCWarning(KWIN_CORE) << "Atomic output test failed on present.";
            wlr_output_rollback(base.native);
            return;
        }
        if (!wlr_output_commit(base.native)) {
            qCWarning(KWIN_CORE) << "Atomic output commit failed on present.";
            return;
        }
#endif
    }

    Output& output;
    wlr_renderer* renderer;

    std::unique_ptr<QImage> buffer;

private:
    QImage::Format pixman_to_qt_image_format(pixman_format_code_t format)
    {
        switch (format) {
        case PIXMAN_a8r8g8b8:
            return QImage::Format_ARGB32_Premultiplied;
        case PIXMAN_x8r8g8b8:
            return QImage::Format_RGB32;
        case PIXMAN_r8g8b8a8:
            return QImage::Format_RGBA8888_Premultiplied;
        case PIXMAN_r8g8b8x8:
            return QImage::Format_RGBX8888;
        case PIXMAN_r8g8b8:
            return QImage::Format_RGB888;
        case PIXMAN_b8g8r8:
            return QImage::Format_BGR888;
        default:
            return QImage::Format_RGBA8888;
        }
    }

#if WLR_HAVE_NEW_PIXEL_COPY_API
    wlr_render_pass* current_render_pass{nullptr};
#endif
};

}
