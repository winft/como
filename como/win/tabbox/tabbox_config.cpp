/*
SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tabbox_config.h"

namespace como
{
namespace win
{
class tabbox_config_private
{
public:
    tabbox_config_private()
        : show_tabbox(tabbox_config::default_show_tabbox())
        , highlight_windows(tabbox_config::default_highlight_window())
        , client_desktop_mode(tabbox_config::default_desktop_mode())
        , client_applications_mode(tabbox_config::default_applications_mode())
        , client_minimized_mode(tabbox_config::default_minimized_mode())
        , show_desktop_mode(tabbox_config::default_show_desktop_mode())
        , client_multi_screen_mode(tabbox_config::default_multi_screen_mode())
        , client_switching_mode(tabbox_config::default_switching_mode())
        , layout_name(tabbox_config::default_layout_name())
    {
    }
    ~tabbox_config_private()
    {
    }
    bool show_tabbox;
    bool highlight_windows;

    tabbox_config::ClientDesktopMode client_desktop_mode;
    tabbox_config::ClientApplicationsMode client_applications_mode;
    tabbox_config::ClientMinimizedMode client_minimized_mode;
    tabbox_config::ShowDesktopMode show_desktop_mode;
    tabbox_config::ClientMultiScreenMode client_multi_screen_mode;
    tabbox_config::ClientSwitchingMode client_switching_mode;
    QString layout_name;
};

tabbox_config::tabbox_config()
    : d(new tabbox_config_private)
{
}

tabbox_config::~tabbox_config()
{
    delete d;
}

tabbox_config::tabbox_config(tabbox_config const& other)
{
    *this = other;
}

tabbox_config& tabbox_config::operator=(tabbox_config const& object)
{
    if (this == &object) {
        return *this;
    }

    if (!d) {
        d = new tabbox_config_private();
    }

    *d = *object.d;
    return *this;
}

tabbox_config::tabbox_config(tabbox_config&& other) noexcept
{
    *this = std::move(other);
}

tabbox_config& tabbox_config::operator=(tabbox_config&& other) noexcept
{
    if (this != &other) {
        d = other.d;
        other.d = nullptr;
    }
    return *this;
}

void tabbox_config::set_highlight_windows(bool highlight)
{
    d->highlight_windows = highlight;
}

bool tabbox_config::is_highlight_windows() const
{
    return d->highlight_windows;
}

void tabbox_config::set_show_tabbox(bool show)
{
    d->show_tabbox = show;
}

bool tabbox_config::is_show_tabbox() const
{
    return d->show_tabbox;
}

tabbox_config::ClientDesktopMode tabbox_config::client_desktop_mode() const
{
    return d->client_desktop_mode;
}

void tabbox_config::set_client_desktop_mode(ClientDesktopMode desktop_mode)
{
    d->client_desktop_mode = desktop_mode;
}

tabbox_config::ClientApplicationsMode tabbox_config::client_applications_mode() const
{
    return d->client_applications_mode;
}

void tabbox_config::set_client_applications_mode(ClientApplicationsMode applications_mode)
{
    d->client_applications_mode = applications_mode;
}

tabbox_config::ClientMinimizedMode tabbox_config::client_minimized_mode() const
{
    return d->client_minimized_mode;
}

void tabbox_config::set_client_minimized_mode(ClientMinimizedMode minimized_mode)
{
    d->client_minimized_mode = minimized_mode;
}

tabbox_config::ShowDesktopMode tabbox_config::show_desktop_mode() const
{
    return d->show_desktop_mode;
}

void tabbox_config::set_show_desktop_mode(ShowDesktopMode show_desktop_mode)
{
    d->show_desktop_mode = show_desktop_mode;
}

tabbox_config::ClientMultiScreenMode tabbox_config::client_multi_screen_mode() const
{
    return d->client_multi_screen_mode;
}

void tabbox_config::set_client_multi_screen_mode(ClientMultiScreenMode multi_screen_mode)
{
    d->client_multi_screen_mode = multi_screen_mode;
}

tabbox_config::ClientSwitchingMode tabbox_config::client_switching_mode() const
{
    return d->client_switching_mode;
}

void tabbox_config::set_client_switching_mode(ClientSwitchingMode switching_mode)
{
    d->client_switching_mode = switching_mode;
}

QString& tabbox_config::layout_name() const
{
    return d->layout_name;
}

void tabbox_config::set_layout_name(const QString& name)
{
    d->layout_name = name;
}

} // namespace win
}
