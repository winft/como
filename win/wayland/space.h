/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "workspace.h"

#include <kwin_export.h>
#include <memory>

namespace KWin
{
class VirtualDesktop;

namespace win
{
namespace x11
{
class window;
}

namespace wayland
{
class window;
struct xdg_activation;

class KWIN_EXPORT space : public Workspace
{
    Q_OBJECT
public:
    space();
    ~space() override;

    QRect get_icon_geometry(Toplevel const* win) const override;

    std::unique_ptr<win::wayland::xdg_activation> activation;

protected:
    void update_space_area_from_windows(QRect const& desktop_area,
                                        std::vector<QRect> const& screens_geos,
                                        win::space_areas& areas) override;

private:
    void handle_window_added(wayland::window* window);
    void handle_window_removed(wayland::window* window);

    void handle_x11_window_added(x11::window* window);

    void handle_desktop_removed(VirtualDesktop* desktop);
};

}
}
}
