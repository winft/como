/*
SPDX-FileCopyrightText: 2011 Arthur Arlt <a.arlt@stud.uni-heidelberg.de>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "base/x11/event_filter.h"
#include "base/x11/xcb/extensions.h"
#include "base/x11/xcb/helpers.h"
#include "base/x11/xcb/proto.h"
#include "render/compositor.h"

#include <QRegion>
#include <QTimer>
#include <xcb/xcb.h>

namespace como::render::x11
{

template<typename Compositor>
class overlay_window : public base::x11::event_filter
{
public:
    explicit overlay_window(Compositor& compositor)
        : base::x11::event_filter(*compositor.base.x11_event_filters,
                                  QVector<int>{XCB_EXPOSE, XCB_VISIBILITY_NOTIFY})
        , m_shown(false)
        , m_window(XCB_WINDOW_NONE)
        , compositor{compositor}
    {
    }

    /// Creates XComposite overlay window, call initOverlay() afterwards
    bool create()
    {
        Q_ASSERT(m_window == XCB_WINDOW_NONE);

        if (!base::x11::xcb::extensions::self()->is_composite_overlay_available()) {
            return false;
        }
        if (!base::x11::xcb::extensions::self()->is_shape_input_available()) {
            // needed in setupOverlay()
            return false;
        }

        base::x11::xcb::overlay_window overlay(compositor.base.x11_data.connection,
                                               compositor.base.x11_data.root_window);
        if (overlay.is_null()) {
            return false;
        }
        m_window = overlay->overlay_win;
        if (m_window == XCB_WINDOW_NONE) {
            return false;
        }
        resize(compositor.base.topology.size);
        return true;
    }

    /// Init overlay and the destination window in it
    void setup(xcb_window_t window)
    {
        Q_ASSERT(m_window != XCB_WINDOW_NONE);
        Q_ASSERT(base::x11::xcb::extensions::self()->is_shape_input_available());

        setNoneBackgroundPixmap(m_window);
        m_shape = QRegion();
        setShape(QRect({}, compositor.base.topology.size));
        if (window != XCB_WINDOW_NONE) {
            setNoneBackgroundPixmap(window);
            setupInputShape(window);
        }
        const uint32_t eventMask = XCB_EVENT_MASK_VISIBILITY_CHANGE;
        xcb_change_window_attributes(
            compositor.base.x11_data.connection, m_window, XCB_CW_EVENT_MASK, &eventMask);
    }

    void show()
    {
        Q_ASSERT(m_window != XCB_WINDOW_NONE);
        if (m_shown)
            return;
        xcb_map_subwindows(compositor.base.x11_data.connection, m_window);
        xcb_map_window(compositor.base.x11_data.connection, m_window);
        m_shown = true;
    }

    /// Hides and resets overlay window
    void hide()
    {
        Q_ASSERT(m_window != XCB_WINDOW_NONE);
        xcb_unmap_window(compositor.base.x11_data.connection, m_window);
        m_shown = false;
        setShape(QRect({}, compositor.base.topology.size));
    }

    void setShape(const QRegion& reg)
    {
        // Avoid setting the same shape again, it causes flicker (apparently it is not a no-op
        // and triggers something).
        if (reg == m_shape)
            return;
        auto const xrects = base::x11::xcb::qt_region_to_rects(reg);
        xcb_shape_rectangles(compositor.base.x11_data.connection,
                             XCB_SHAPE_SO_SET,
                             XCB_SHAPE_SK_BOUNDING,
                             XCB_CLIP_ORDERING_UNSORTED,
                             m_window,
                             0,
                             0,
                             xrects.count(),
                             xrects.data());
        setupInputShape(m_window);
        m_shape = reg;
    }

    void resize(const QSize& size)
    {
        Q_ASSERT(m_window != XCB_WINDOW_NONE);
        const uint32_t geometry[2]
            = {static_cast<uint32_t>(size.width()), static_cast<uint32_t>(size.height())};
        xcb_configure_window(compositor.base.x11_data.connection,
                             m_window,
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                             geometry);
        setShape(QRegion(0, 0, size.width(), size.height()));
    }

    /// Destroys XComposite overlay window
    void destroy()
    {
        if (m_window == XCB_WINDOW_NONE)
            return;
        // reset the overlay shape
        auto const& space_size = compositor.base.topology.size;
        xcb_rectangle_t rec = {0,
                               0,
                               static_cast<uint16_t>(space_size.width()),
                               static_cast<uint16_t>(space_size.height())};
        xcb_shape_rectangles(compositor.base.x11_data.connection,
                             XCB_SHAPE_SO_SET,
                             XCB_SHAPE_SK_BOUNDING,
                             XCB_CLIP_ORDERING_UNSORTED,
                             m_window,
                             0,
                             0,
                             1,
                             &rec);
        xcb_shape_rectangles(compositor.base.x11_data.connection,
                             XCB_SHAPE_SO_SET,
                             XCB_SHAPE_SK_INPUT,
                             XCB_CLIP_ORDERING_UNSORTED,
                             m_window,
                             0,
                             0,
                             1,
                             &rec);
        xcb_composite_release_overlay_window(compositor.base.x11_data.connection, m_window);

        m_window = XCB_WINDOW_NONE;
        m_shown = false;
    }

    xcb_window_t window() const
    {
        return m_window;
    }

    bool event(xcb_generic_event_t* event) override
    {
        const uint8_t eventType = event->response_type & ~0x80;
        if (eventType == XCB_EXPOSE) {
            const auto* expose = reinterpret_cast<xcb_expose_event_t*>(event);

            // root window needs repainting or overlay needs repainting
            if (expose->window == compositor.base.x11_data.root_window
                || (m_window != XCB_WINDOW_NONE && expose->window == m_window)) {
                compositor.addRepaint(QRegion(expose->x, expose->y, expose->width, expose->height));
            }
        } else if (eventType == XCB_VISIBILITY_NOTIFY) {
            const auto* visibility = reinterpret_cast<xcb_visibility_notify_event_t*>(event);
            if (m_window != XCB_WINDOW_NONE && visibility->window == m_window) {
                bool was_visible = visible;
                visible = (visibility->state != XCB_VISIBILITY_FULLY_OBSCURED);
                if (!was_visible && visible) {
                    // hack for #154825
                    full_repaint(compositor);
                    QTimer::singleShot(2000, compositor.qobject.get(), [comp = &compositor] {
                        full_repaint(*comp);
                    });
                }
                compositor.schedule_repaint();
            }
        }
        return false;
    }

    bool visible{true};

private:
    void setNoneBackgroundPixmap(xcb_window_t window)
    {
        const uint32_t mask = XCB_BACK_PIXMAP_NONE;
        xcb_change_window_attributes(
            compositor.base.x11_data.connection, window, XCB_CW_BACK_PIXMAP, &mask);
    }

    void setupInputShape(xcb_window_t window)
    {
        xcb_shape_rectangles(compositor.base.x11_data.connection,
                             XCB_SHAPE_SO_SET,
                             XCB_SHAPE_SK_INPUT,
                             XCB_CLIP_ORDERING_UNSORTED,
                             window,
                             0,
                             0,
                             0,
                             nullptr);
    }

    // For showOverlay()
    bool m_shown;

    QRegion m_shape;
    xcb_window_t m_window;
    Compositor& compositor;
};

}
