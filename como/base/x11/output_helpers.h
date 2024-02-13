/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "output.h"
#include "xcb/randr.h"

#include <memory>
#include <vector>
#include <xcb/randr.h>

namespace como::base::x11
{

template<typename Base, typename Resources>
auto get_outputs_from_resources(Base& base, Resources resources)
    -> std::vector<std::unique_ptr<typename Base::output_t>>
{
    using output_t = typename Base::output_t;

    if (resources.is_null()) {
        return {};
    }

    std::vector<std::unique_ptr<output_t>> outputs;

    xcb_randr_crtc_t* crtcs = resources.crtcs();
    xcb_randr_mode_info_t* modes = resources.modes();

    std::vector<xcb::randr::crtc_info> crtc_infos;
    for (int i = 0; i < resources->num_crtcs; ++i) {
        crtc_infos.push_back(
            xcb::randr::crtc_info(base.x11_data.connection, crtcs[i], resources->config_timestamp));
    }

    for (int i = 0; i < resources->num_crtcs; ++i) {
        xcb::randr::crtc_info crtc_info(crtc_infos.at(i));

        auto randr_outputs = crtc_info.outputs();
        std::vector<xcb::randr::output_info> output_infos;

        if (randr_outputs) {
            for (int i = 0; i < resources->num_outputs; ++i) {
                output_infos.push_back(xcb::randr::output_info(
                    base.x11_data.connection, randr_outputs[i], resources->config_timestamp));
            }
        }

        float refresh_rate = -1.0f;
        for (int j = 0; j < resources->num_modes; ++j) {
            if (crtc_info->mode == modes[j].id) {
                if (modes[j].htotal != 0 && modes[j].vtotal != 0) {
                    // BUG 313996, refresh rate calculation
                    int dotclock = modes[j].dot_clock, vtotal = modes[j].vtotal;
                    if (modes[j].mode_flags & XCB_RANDR_MODE_FLAG_INTERLACE) {
                        dotclock *= 2;
                    }
                    if (modes[j].mode_flags & XCB_RANDR_MODE_FLAG_DOUBLE_SCAN) {
                        vtotal *= 2;
                    }
                    refresh_rate = dotclock / float(modes[j].htotal * vtotal);
                }
                // Found mode.
                break;
            }
        }

        auto const geo = crtc_info.rect();
        if (geo.isValid()) {
            xcb_randr_crtc_t crtc = crtcs[i];

            // TODO: Perhaps the output has to save the inherited gamma ramp and
            // restore it during tear down. Currently neither standalone x11 nor
            // drm platform do this.
            xcb::randr::crtc_gamma gamma(base.x11_data.connection, crtc);

            auto output = std::make_unique<output_t>(base);
            output->data.crtc = crtc;
            output->data.gamma_ramp_size = gamma.is_null() ? 0 : gamma->size;
            output->data.geometry = geo;
            output->data.refresh_rate = refresh_rate * 1000;

            for (int j = 0; j < crtc_info->num_outputs; ++j) {
                xcb::randr::output_info output_info(output_infos.at(j));
                if (output_info->crtc != crtc) {
                    continue;
                }

                auto physical_size = QSize(output_info->mm_width, output_info->mm_height);

                switch (crtc_info->rotation) {
                case XCB_RANDR_ROTATION_ROTATE_0:
                case XCB_RANDR_ROTATION_ROTATE_180:
                    break;
                case XCB_RANDR_ROTATION_ROTATE_90:
                case XCB_RANDR_ROTATION_ROTATE_270:
                    physical_size.transpose();
                    break;
                case XCB_RANDR_ROTATION_REFLECT_X:
                case XCB_RANDR_ROTATION_REFLECT_Y:
                    break;
                }

                output->data.name = output_info.name();
                output->data.physical_size = physical_size;
                break;
            }

            outputs.push_back(std::move(output));
        }
    }

    return outputs;
}

}
