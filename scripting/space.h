/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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

#include "window.h"

#include "base/platform.h"
#include "screens.h"
#include "win/virtual_desktops.h"
#include "win/wayland/window.h"
#include "win/x11/window.h"

#include <QObject>
#include <QQmlListProperty>
#include <QRect>
#include <QSize>
#include <QStringList>

#include <kwinglobals.h>

#include <memory>
#include <vector>

namespace KWin::scripting
{

class space : public QObject
{
    Q_OBJECT
    Q_PROPERTY(
        int currentDesktop READ currentDesktop WRITE setCurrentDesktop NOTIFY currentDesktopChanged)
    Q_PROPERTY(KWin::scripting::window* activeClient READ activeClient WRITE setActiveClient NOTIFY
                   clientActivated)
    // TODO: write and notify?
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize NOTIFY desktopLayoutChanged)
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth NOTIFY desktopLayoutChanged)
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight NOTIFY desktopLayoutChanged)
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)
    Q_PROPERTY(QSize workspaceSize READ workspaceSize)
    /**
     * The number of desktops currently used. Minimum number of desktops is 1, maximum 20.
     */
    Q_PROPERTY(
        int desktops READ numberOfDesktops WRITE setNumberOfDesktops NOTIFY numberDesktopsChanged)
    /**
     * The same of the display, that is all screens.
     * @deprecated since 5.0 use virtualScreenSize
     */
    Q_PROPERTY(QSize displaySize READ displaySize)
    /**
     * The width of the display, that is width of all combined screens.
     * @deprecated since 5.0 use virtualScreenSize
     */
    Q_PROPERTY(int displayWidth READ displayWidth)
    /**
     * The height of the display, that is height of all combined screens.
     * @deprecated since 5.0 use virtualScreenSize
     */
    Q_PROPERTY(int displayHeight READ displayHeight)
    Q_PROPERTY(int activeScreen READ activeScreen)
    Q_PROPERTY(int numScreens READ numScreens NOTIFY numberScreensChanged)
    Q_PROPERTY(QString currentActivity READ currentActivity WRITE setCurrentActivity NOTIFY
                   currentActivityChanged)
    Q_PROPERTY(QStringList activities READ activityList NOTIFY activitiesChanged)
    /**
     * The bounding size of all screens combined. Overlapping areas
     * are not counted multiple times.
     * @see virtualScreenGeometry
     */
    Q_PROPERTY(QSize virtualScreenSize READ virtualScreenSize NOTIFY virtualScreenSizeChanged)
    /**
     * The bounding geometry of all outputs combined. Always starts at (0,0) and has
     * virtualScreenSize as it's size.
     * @see virtualScreenSize
     */
    Q_PROPERTY(
        QRect virtualScreenGeometry READ virtualScreenGeometry NOTIFY virtualScreenGeometryChanged)

public:
    //------------------------------------------------------------------
    // enums copy&pasted from kwinglobals.h because qtscript is evil

    enum ClientAreaOption {
        ///< geometry where a window will be initially placed after being mapped
        PlacementArea,
        ///< window movement snapping area?  ignore struts
        MovementArea,
        ///< geometry to which a window will be maximized
        MaximizeArea,
        ///< like MaximizeArea, but ignore struts - used e.g. for topmenu
        MaximizeFullArea,
        ///< area for fullscreen windows
        FullScreenArea,
        ///< whole workarea (all screens together)
        WorkArea,
        ///< whole area (all screens together), ignore struts
        FullArea,
        ///< one whole screen, ignore struts
        ScreenArea
    };
    Q_ENUM(ClientAreaOption)

    enum ElectricBorder {
        ElectricTop,
        ElectricTopRight,
        ElectricRight,
        ElectricBottomRight,
        ElectricBottom,
        ElectricBottomLeft,
        ElectricLeft,
        ElectricTopLeft,
        ELECTRIC_COUNT,
        ElectricNone
    };
    Q_ENUM(ElectricBorder)

#define GETTERSETTERDEF(rettype, getter, setter)                                                   \
    rettype getter() const;                                                                        \
    void setter(rettype val);
    GETTERSETTERDEF(int, numberOfDesktops, setNumberOfDesktops)
    GETTERSETTERDEF(int, currentDesktop, setCurrentDesktop)
    GETTERSETTERDEF(QString, currentActivity, setCurrentActivity)
#undef GETTERSETTERDEF

    virtual window* activeClient() const = 0;

    virtual void setActiveClient(window* win) = 0;

    QSize desktopGridSize() const;
    int desktopGridWidth() const;
    int desktopGridHeight() const;
    int workspaceWidth() const;
    int workspaceHeight() const;
    QSize workspaceSize() const;
    int displayWidth() const;
    int displayHeight() const;
    QSize displaySize() const;
    int activeScreen() const;
    int numScreens() const;
    QStringList activityList() const;
    QSize virtualScreenSize() const;
    QRect virtualScreenGeometry() const;

    virtual std::vector<window*> windows() const = 0;

    /**
     * Returns the geometry a Client can use with the specified option.
     * This method should be preferred over other methods providing screen sizes as the
     * various options take constraints such as struts set on panels into account.
     * This method is also multi screen aware, but there are also options to get full areas.
     * @param option The type of area which should be considered
     * @param screen The screen for which the area should be considered
     * @param desktop The desktop for which the area should be considered, in general there should
     * not be a difference
     * @returns The specified screen geometry
     */
    Q_SCRIPTABLE QRect clientArea(ClientAreaOption option, int screen, int desktop) const
    {
        return client_area_impl(static_cast<clientAreaOption>(option), screen, desktop);
    }

    /**
     * Overloaded method for convenience.
     * @param option The type of area which should be considered
     * @param point The coordinates which have to be included in the area
     * @param desktop The desktop for which the area should be considered, in general there should
     * not be a difference
     * @returns The specified screen geometry
     */
    Q_SCRIPTABLE QRect clientArea(ClientAreaOption option, QPoint const& point, int desktop) const
    {
        return client_area_impl(static_cast<clientAreaOption>(option), point, desktop);
    }

    /**
     * Overloaded method for convenience.
     * @param client The Client for which the area should be retrieved
     * @returns The specified screen geometry
     */
    Q_SCRIPTABLE QRect clientArea(ClientAreaOption option, KWin::scripting::window* window) const
    {
        return client_area_impl(static_cast<clientAreaOption>(option), window);
    }

    Q_SCRIPTABLE QRect clientArea(ClientAreaOption option,
                                  KWin::scripting::window const* window) const
    {
        return client_area_impl(static_cast<clientAreaOption>(option), window);
    }

    /**
     * Returns the name for the given @p desktop.
     */
    Q_SCRIPTABLE QString desktopName(int desktop) const;
    /**
     * Create a new virtual desktop at the requested position.
     * @param position The position of the desktop. It should be in range [0, count].
     * @param name The name for the new desktop, if empty the default name will be used.
     */
    Q_SCRIPTABLE void createDesktop(int position, const QString& name) const;
    /**
     * Remove the virtual desktop at the requested position
     * @param position The position of the desktop to be removed. It should be in range [0, count -
     * 1].
     */
    Q_SCRIPTABLE void removeDesktop(int position) const;

    /**
     * Provides support information about the currently running KWin instance.
     */
    virtual Q_SCRIPTABLE QString supportInformation() const = 0;

    /**
     * Finds the Client with the given @p windowId.
     * @param windowId The window Id of the Client
     * @return The found Client or @c null
     */
    Q_SCRIPTABLE KWin::scripting::window* getClient(qulonglong windowId);

public Q_SLOTS:
    /// All the available key bindings.
#define SWITCH_VD_SLOT(name, direction)                                                            \
    void name()                                                                                    \
    {                                                                                              \
        win::virtual_desktop_manager::self()->moveTo<direction>(options->isRollOverDesktops());    \
    }

    SWITCH_VD_SLOT(slotSwitchDesktopNext, win::virtual_desktop_next)
    SWITCH_VD_SLOT(slotSwitchDesktopPrevious, win::virtual_desktop_previous)
    SWITCH_VD_SLOT(slotSwitchDesktopRight, win::virtual_desktop_right)
    SWITCH_VD_SLOT(slotSwitchDesktopLeft, win::virtual_desktop_left)
    SWITCH_VD_SLOT(slotSwitchDesktopUp, win::virtual_desktop_above)
    SWITCH_VD_SLOT(slotSwitchDesktopDown, win::virtual_desktop_below)

    virtual void slotSwitchToNextScreen() = 0;
    virtual void slotWindowToNextScreen() = 0;
    virtual void slotToggleShowDesktop() = 0;

    virtual void slotWindowMaximize() = 0;
    virtual void slotWindowMaximizeVertical() = 0;
    virtual void slotWindowMaximizeHorizontal() = 0;
    virtual void slotWindowMinimize() = 0;

    /// Deprecated
    void slotWindowShade()
    {
    }

    virtual void slotWindowRaise() = 0;
    virtual void slotWindowLower() = 0;
    virtual void slotWindowRaiseOrLower() = 0;
    virtual void slotActivateAttentionWindow() = 0;

    virtual void slotWindowPackLeft() = 0;
    virtual void slotWindowPackRight() = 0;
    virtual void slotWindowPackUp() = 0;
    virtual void slotWindowPackDown() = 0;

    virtual void slotWindowGrowHorizontal() = 0;
    virtual void slotWindowGrowVertical() = 0;
    virtual void slotWindowShrinkHorizontal() = 0;
    virtual void slotWindowShrinkVertical() = 0;

    virtual void slotWindowQuickTileLeft() = 0;
    virtual void slotWindowQuickTileRight() = 0;
    virtual void slotWindowQuickTileTop() = 0;
    virtual void slotWindowQuickTileBottom() = 0;
    virtual void slotWindowQuickTileTopLeft() = 0;
    virtual void slotWindowQuickTileTopRight() = 0;
    virtual void slotWindowQuickTileBottomLeft() = 0;
    virtual void slotWindowQuickTileBottomRight() = 0;

    virtual void slotSwitchWindowUp() = 0;
    virtual void slotSwitchWindowDown() = 0;
    virtual void slotSwitchWindowRight() = 0;
    virtual void slotSwitchWindowLeft() = 0;

    virtual void slotIncreaseWindowOpacity() = 0;
    virtual void slotLowerWindowOpacity() = 0;

    virtual void slotWindowOperations() = 0;
    virtual void slotWindowClose() = 0;
    virtual void slotWindowMove() = 0;
    virtual void slotWindowResize() = 0;
    virtual void slotWindowAbove() = 0;
    virtual void slotWindowBelow() = 0;
    virtual void slotWindowOnAllDesktops() = 0;
    virtual void slotWindowFullScreen() = 0;
    virtual void slotWindowNoBorder() = 0;

    virtual void slotWindowToNextDesktop() = 0;
    virtual void slotWindowToPreviousDesktop() = 0;
    virtual void slotWindowToDesktopRight() = 0;
    virtual void slotWindowToDesktopLeft() = 0;
    virtual void slotWindowToDesktopUp() = 0;
    virtual void slotWindowToDesktopDown() = 0;

#undef SIMPLE_SLOT
#undef QUICKTILE_SLOT
#undef SWITCH_WINDOW_SLOT
#undef SWITCH_VD_SLOT

    /**
     * Sends the window to the given @p screen.
     */
    virtual void sendClientToScreen(KWin::scripting::window* client, int screen) = 0;

    /**
     * Shows an outline at the specified @p geometry.
     * If an outline is already shown the outline is moved to the new position.
     * Use hideOutline to remove the outline again.
     */
    void showOutline(QRect const& geometry);
    /**
     * Overloaded method for convenience.
     */
    void showOutline(int x, int y, int width, int height);
    /**
     * Hides the outline previously shown by showOutline.
     */
    void hideOutline();

    window* get_window(Toplevel* client) const;

    void handle_client_added(Toplevel* client);
    void handle_client_removed(Toplevel* client);

Q_SIGNALS:
    void desktopPresenceChanged(KWin::scripting::window* client, int desktop);
    void currentDesktopChanged(int desktop, KWin::scripting::window* client);
    void clientAdded(KWin::scripting::window* client);
    void clientRemoved(KWin::scripting::window* client);

    /// Deprecated
    void clientManaging(KWin::scripting::window* client);

    void clientMinimized(KWin::scripting::window* client);
    void clientUnminimized(KWin::scripting::window* client);
    void clientRestored(KWin::scripting::window* client);
    void clientMaximizeSet(KWin::scripting::window* client, bool h, bool v);
    void killWindowCalled(KWin::scripting::window* client);
    void clientActivated(KWin::scripting::window* client);
    void clientFullScreenSet(KWin::scripting::window* client, bool fullScreen, bool user);
    void clientSetKeepAbove(KWin::scripting::window* client, bool keepAbove);
    /**
     * Signal emitted whenever the number of desktops changed.
     * To get the current number of desktops use the property desktops.
     * @param oldNumberOfDesktops The previous number of desktops.
     */
    void numberDesktopsChanged(uint oldNumberOfDesktops);
    /**
     * Signal emitted whenever the layout of virtual desktops changed.
     * That is desktopGrid(Size/Width/Height) will have new values.
     * @since 4.11
     */
    void desktopLayoutChanged();
    /**
     * The demands attention state for Client @p c changed to @p set.
     * @param c The Client for which demands attention changed
     * @param set New value of demands attention
     */
    void clientDemandsAttentionChanged(KWin::scripting::window* window, bool set);
    /**
     * Signal emitted when the number of screens changes.
     * @param count The new number of screens
     */
    void numberScreensChanged(int count);
    /**
     * This signal is emitted when the size of @p screen changes.
     * Don't forget to fetch an updated client area.
     *
     * @deprecated Use QScreen::geometryChanged signal instead.
     */
    void screenResized(int screen);
    /**
     * Signal emitted whenever the current activity changed.
     * @param id id of the new activity
     */
    void currentActivityChanged(const QString& id);
    /**
     * Signal emitted whenever the list of activities changed.
     * @param id id of the new activity
     */
    void activitiesChanged(const QString& id);
    /**
     * This signal is emitted when a new activity is added
     * @param id id of the new activity
     */
    void activityAdded(const QString& id);
    /**
     * This signal is emitted when the activity
     * is removed
     * @param id id of the removed activity
     */
    void activityRemoved(const QString& id);
    /**
     * Emitted whenever the virtualScreenSize changes.
     * @see virtualScreenSize()
     * @since 5.0
     */
    void virtualScreenSizeChanged();
    /**
     * Emitted whenever the virtualScreenGeometry changes.
     * @see virtualScreenGeometry()
     * @since 5.0
     */
    void virtualScreenGeometryChanged();

protected:
    space() = default;

    virtual QRect client_area_impl(clientAreaOption option, int screen, int desktop) const = 0;
    virtual QRect
    client_area_impl(clientAreaOption option, QPoint const& point, int desktop) const = 0;
    virtual QRect client_area_impl(clientAreaOption option, window* window) const = 0;
    virtual QRect client_area_impl(clientAreaOption option, window const* window) const = 0;

    virtual window* get_client_impl(qulonglong windowId) = 0;

    // TODO: make this private. Remove dynamic inheritance?
    std::vector<std::unique_ptr<window>> m_windows;
    int windows_count{0};

private:
    Q_DISABLE_COPY(space)

    void setupAbstractClientConnections(window* window);
    void setupClientConnections(window* window);
};

class qt_script_space : public space
{
    Q_OBJECT

public:
    qt_script_space() = default;
    /**
     * List of Clients currently managed by KWin.
     */
    Q_INVOKABLE QList<KWin::scripting::window*> clientList() const;
};

class declarative_script_space : public space
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<KWin::scripting::window> clients READ clients)

public:
    declarative_script_space() = default;

    QQmlListProperty<KWin::scripting::window> clients();
    static int countClientList(QQmlListProperty<KWin::scripting::window>* clients);
    static window* atClientList(QQmlListProperty<KWin::scripting::window>* clients, int index);
};

// TODO Plasma 6: Remove it.
void connect_legacy_screen_resize(space* receiver);

template<typename Space, typename RefSpace>
class template_space : public Space
{
public:
    template_space(RefSpace* ref_space)
        : ref_space{ref_space}
    {
        auto vds = win::virtual_desktop_manager::self();

        QObject::connect(
            ref_space, &RefSpace::desktopPresenceChanged, this, [this](auto client, auto desktop) {
                auto window = Space::get_window(client);
                Q_EMIT Space::desktopPresenceChanged(window, desktop);
            });

        QObject::connect(
            ref_space, &RefSpace::currentDesktopChanged, this, [this](auto desktop, auto client) {
                auto window = Space::get_window(client);
                Q_EMIT Space::currentDesktopChanged(desktop, window);
            });

        QObject::connect(ref_space, &RefSpace::clientAdded, this, &Space::handle_client_added);
        QObject::connect(ref_space, &RefSpace::clientRemoved, this, &Space::handle_client_removed);
        QObject::connect(
            ref_space, &RefSpace::wayland_window_added, this, &Space::handle_client_added);

        QObject::connect(ref_space, &RefSpace::clientActivated, this, [this](auto client) {
            auto window = Space::get_window(client);
            Q_EMIT Space::clientActivated(window);
        });

        QObject::connect(ref_space,
                         &RefSpace::clientDemandsAttentionChanged,
                         this,
                         [this](auto client, auto set) {
                             auto window = Space::get_window(client);
                             Q_EMIT Space::clientDemandsAttentionChanged(window, set);
                         });

        QObject::connect(
            vds, &win::virtual_desktop_manager::countChanged, this, &space::numberDesktopsChanged);
        QObject::connect(
            vds, &win::virtual_desktop_manager::layoutChanged, this, &space::desktopLayoutChanged);

        auto& screens = kwinApp()->get_base().screens;
        QObject::connect(&screens, &Screens::sizeChanged, this, &space::virtualScreenSizeChanged);
        QObject::connect(
            &screens, &Screens::geometryChanged, this, &space::virtualScreenGeometryChanged);
        QObject::connect(&screens,
                         &Screens::countChanged,
                         this,
                         [this](int /*previousCount*/, int currentCount) {
                             Q_EMIT Space::numberScreensChanged(currentCount);
                         });

        connect_legacy_screen_resize(this);

        for (auto client : ref_space->allClientList()) {
            Space::handle_client_added(client);
        }
    }

    std::vector<window*> windows() const override
    {
        std::vector<window*> ret;
        for (auto const& window : ref_space->m_windows) {
            if (window->control && window->control->scripting) {
                ret.push_back(window->control->scripting.get());
            }
        }
        return ret;
    }

    window* activeClient() const override
    {
        auto active_client = ref_space->activeClient();
        if (!active_client) {
            return nullptr;
        }
        return Space::get_window(active_client);
    }

    void setActiveClient(window* win) override
    {
        ref_space->activateClient(win->client());
    }

    void sendClientToScreen(KWin::scripting::window* client, int screen) override
    {
        if (screen < 0 || screen >= kwinApp()->get_base().screens.count()) {
            return;
        }
        ref_space->sendClientToScreen(client->client(), screen);
    }

#define SIMPLE_SLOT(name)                                                                          \
    void name() override                                                                           \
    {                                                                                              \
        ref_space->name();                                                                         \
    }

#define QUICKTILE_SLOT(name, modes)                                                                \
    void name() override                                                                           \
    {                                                                                              \
        ref_space->quickTileWindow(modes);                                                         \
    }

#define SWITCH_WINDOW_SLOT(name, direction)                                                        \
    void name() override                                                                           \
    {                                                                                              \
        ref_space->switchWindow(win::space::direction);                                            \
    }

    SIMPLE_SLOT(slotSwitchToNextScreen)
    SIMPLE_SLOT(slotWindowToNextScreen)
    SIMPLE_SLOT(slotToggleShowDesktop)

    SIMPLE_SLOT(slotWindowMaximize)
    SIMPLE_SLOT(slotWindowMaximizeVertical)
    SIMPLE_SLOT(slotWindowMaximizeHorizontal)
    SIMPLE_SLOT(slotWindowMinimize)

    SIMPLE_SLOT(slotWindowRaise)
    SIMPLE_SLOT(slotWindowLower)
    SIMPLE_SLOT(slotWindowRaiseOrLower)
    SIMPLE_SLOT(slotActivateAttentionWindow)

    SIMPLE_SLOT(slotWindowPackLeft)
    SIMPLE_SLOT(slotWindowPackRight)
    SIMPLE_SLOT(slotWindowPackUp)
    SIMPLE_SLOT(slotWindowPackDown)

    SIMPLE_SLOT(slotWindowGrowHorizontal)
    SIMPLE_SLOT(slotWindowGrowVertical)
    SIMPLE_SLOT(slotWindowShrinkHorizontal)
    SIMPLE_SLOT(slotWindowShrinkVertical)

    QUICKTILE_SLOT(slotWindowQuickTileLeft, win::quicktiles::left)
    QUICKTILE_SLOT(slotWindowQuickTileRight, win::quicktiles::right)
    QUICKTILE_SLOT(slotWindowQuickTileTop, win::quicktiles::top)
    QUICKTILE_SLOT(slotWindowQuickTileBottom, win::quicktiles::bottom)
    QUICKTILE_SLOT(slotWindowQuickTileTopLeft, win::quicktiles::top | win::quicktiles::left)
    QUICKTILE_SLOT(slotWindowQuickTileTopRight, win::quicktiles::top | win::quicktiles::right)
    QUICKTILE_SLOT(slotWindowQuickTileBottomLeft, win::quicktiles::bottom | win::quicktiles::left)
    QUICKTILE_SLOT(slotWindowQuickTileBottomRight, win::quicktiles::bottom | win::quicktiles::right)

    SWITCH_WINDOW_SLOT(slotSwitchWindowUp, DirectionNorth)
    SWITCH_WINDOW_SLOT(slotSwitchWindowDown, DirectionSouth)
    SWITCH_WINDOW_SLOT(slotSwitchWindowRight, DirectionEast)
    SWITCH_WINDOW_SLOT(slotSwitchWindowLeft, DirectionWest)

    SIMPLE_SLOT(slotIncreaseWindowOpacity)
    SIMPLE_SLOT(slotLowerWindowOpacity)

    SIMPLE_SLOT(slotWindowOperations)
    SIMPLE_SLOT(slotWindowClose)
    SIMPLE_SLOT(slotWindowMove)
    SIMPLE_SLOT(slotWindowResize)
    SIMPLE_SLOT(slotWindowAbove)
    SIMPLE_SLOT(slotWindowBelow)
    SIMPLE_SLOT(slotWindowOnAllDesktops)
    SIMPLE_SLOT(slotWindowFullScreen)
    SIMPLE_SLOT(slotWindowNoBorder)

    SIMPLE_SLOT(slotWindowToNextDesktop)
    SIMPLE_SLOT(slotWindowToPreviousDesktop)
    SIMPLE_SLOT(slotWindowToDesktopRight)
    SIMPLE_SLOT(slotWindowToDesktopLeft)
    SIMPLE_SLOT(slotWindowToDesktopUp)
    SIMPLE_SLOT(slotWindowToDesktopDown)

#undef SIMPLE_SLOT
#undef QUICKTILE_SLOT
#undef SWITCH_WINDOW_SLOT

protected:
    QRect client_area_impl(clientAreaOption option, int screen, int desktop) const override
    {
        return ref_space->clientArea(option, screen, desktop);
    }

    QRect client_area_impl(clientAreaOption option, QPoint const& point, int desktop) const override
    {
        return ref_space->clientArea(option, point, desktop);
    }

    QRect client_area_impl(clientAreaOption option, window* window) const override
    {
        return ref_space->clientArea(option, window->client());
    }

    QRect client_area_impl(clientAreaOption option, window const* window) const override
    {
        return ref_space->clientArea(option, window->client());
    }

    QString supportInformation() const override
    {
        return ref_space->supportInformation();
    }

    window* get_client_impl(qulonglong windowId) override
    {
        for (auto& win : ref_space->m_windows) {
            if (win->control && win->xcb_window() == windowId) {
                return win->control->scripting.get();
            }
        }
        return nullptr;
    }

    RefSpace* ref_space;
};

}
