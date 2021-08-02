/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

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
#ifndef KWIN_FOCUS_CHAIN_H
#define KWIN_FOCUS_CHAIN_H
// KWin
#include <kwinglobals.h>
// Qt
#include <QHash>
#include <QObject>

namespace KWin
{
class Toplevel;

namespace win
{
/**
 * @brief Singleton class to handle the various focus chains.
 *
 * A focus chain is a list of Clients containing information on which Client should be activated.
 *
 * Internally this FocusChain holds multiple independent chains. There is one chain of most recently
 * used Clients which is primarily used by TabBox to build up the list of Clients for navigation.
 * The chains are organized as a normal QList of Clients with the most recently used Client being
 * the last item of the list, that is a LIFO like structure.
 *
 * In addition there is one chain for each virtual desktop which is used to determine which Client
 * should get activated when the user switches to another virtual desktop.
 *
 * Furthermore this class contains various helper methods for the two different kind of chains.
 */
class KWIN_EXPORT FocusChain : public QObject
{
    Q_OBJECT
public:
    enum Change { MakeFirst, MakeLast, Update };
    ~FocusChain() override;
    /**
     * @brief Updates the position of the @p client according to the requested @p change in the
     * focus chain.
     *
     * This method affects both the most recently used focus chain and the per virtual desktop focus
     * chain.
     *
     * In case the client does no longer want to get focus, it is removed from all chains. In case
     * the client is on all virtual desktops it is ensured that it is present in each of the virtual
     * desktops focus chain. In case it's on exactly one virtual desktop it is ensured that it is
     * only in the focus chain for that virtual desktop.
     *
     * Depending on @p change the Client is inserted at different positions in the focus chain. In
     * case of @c MakeFirst it is moved to the first position of the chain, in case of
     * @c MakeLast it is moved to the last position of the chain. In all other cases it
     * depends on whether the @p client is the currently active Client. If it is the active Client
     * it becomes the first Client in the chain, otherwise it is inserted at the second position
     * that is directly after the currently active Client.
     *
     * @param client The Client which should be moved inside the chains.
     * @param change Where to move the Client
     * @return void
     */
    void update(Toplevel* window, Change change);
    /**
     * @brief Moves @p client behind the @p reference Client in all focus chains.
     *
     * @param client The Client to move in the chains
     * @param reference The Client behind which the @p client should be moved
     * @return void
     */
    void moveAfterClient(Toplevel* window, Toplevel* reference);
    /**
     * @brief Finds the best Client to become the new active Client in the focus chain for the given
     * virtual @p desktop.
     *
     * In case that separate screen focus is used only Clients on the current screen are considered.
     * If no Client for activation is found @c null is returned.
     *
     * @param desktop The virtual desktop to look for a Client for activation
     * @return :X11Client *The Client which could be activated or @c null if there is none.
     */
    Toplevel* getForActivation(uint desktop) const;
    /**
     * @brief Finds the best Client to become the new active Client in the focus chain for the given
     * virtual @p desktop on the given @p screen.
     *
     * This method makes only sense to use if separate screen focus is used. If separate screen
     * focus is disabled the @p screen is ignored. If no Client for activation is found @c null is
     * returned.
     *
     * @param desktop The virtual desktop to look for a Client for activation
     * @param screen The screen to constrain the search on with separate screen focus
     * @return :X11Client *The Client which could be activated or @c null if there is none.
     */
    Toplevel* getForActivation(uint desktop, int screen) const;

    /**
     * @brief Checks whether the most recently used focus chain contains the given @p client.
     *
     * Does not consider the per-desktop focus chains.
     * @param client The Client to look for.
     * @return bool @c true if the most recently used focus chain contains @p client, @c false
     * otherwise.
     */
    bool contains(Toplevel* window) const;
    /**
     * @brief Checks whether the focus chain for the given @p desktop contains the given @p client.
     *
     * Does not consider the most recently used focus chain.
     *
     * @param client The Client to look for.
     * @param desktop The virtual desktop whose focus chain should be used
     * @return bool @c true if the focus chain for @p desktop contains @p client, @c false
     * otherwise.
     */
    bool contains(Toplevel* window, uint desktop) const;
    /**
     * @brief Queries the most recently used focus chain for the next Client after the given
     * @p reference Client.
     *
     * The navigation wraps around the borders of the chain. That is if the @p reference Client is
     * the last item of the focus chain, the first Client will be returned.
     *
     * If the @p reference Client cannot be found in the focus chain, the first element of the focus
     * chain is returned.
     *
     * @param reference The start point in the focus chain to search
     * @return :X11Client *The relatively next Client in the most recently used chain.
     */
    Toplevel* nextMostRecentlyUsed(Toplevel* reference) const;
    /**
     * @brief Queries the focus chain for @p desktop for the next Client in relation to the given
     * @p reference Client.
     *
     * The method finds the first usable Client which is not the @p reference Client. If no Client
     * can be found @c null is returned
     *
     * @param reference The reference Client which should not be returned
     * @param desktop The virtual desktop whose focus chain should be used
     * @return :X11Client *The next usable Client or @c null if none can be found.
     */
    Toplevel* nextForDesktop(Toplevel* reference, uint desktop) const;
    /**
     * @brief Returns the first Client in the most recently used focus chain. First Client in this
     * case means really the first Client in the chain and not the most recently used Client.
     *
     * @return :X11Client *The first Client in the most recently used chain.
     */
    Toplevel* firstMostRecentlyUsed() const;

public Q_SLOTS:
    /**
     * @brief Resizes the per virtual desktop focus chains from @p previousSize to @p newSize.
     * This means that for each virtual desktop between previous and new size a new focus chain is
     * created and in case the number is reduced the focus chains are destroyed.
     *
     * @param previousSize The previous number of virtual desktops
     * @param newSize The new number of virtual desktops
     * @return void
     */
    void resize(uint previousSize, uint newSize);
    /**
     * @brief Removes @p client from all focus chains.
     *
     * @param client The Client to remove from all focus chains.
     * @return void
     */
    void remove(Toplevel* window);
    void setSeparateScreenFocus(bool enabled);
    void setActiveClient(Toplevel* window);
    void setCurrentDesktop(uint previous, uint newDesktop);
    bool isUsableFocusCandidate(Toplevel* window, Toplevel* prev) const;

private:
    using Chain = QList<Toplevel*>;
    /**
     * @brief Makes @p client the first Client in the given focus @p chain.
     *
     * This means the existing position of @p client is dropped and @p client is appended to the
     * @p chain which makes it the first item.
     *
     * @param client The Client to become the first in @p chain
     * @param chain The focus chain to operate on
     * @return void
     */
    void makeFirstInChain(Toplevel* window, Chain& chain);
    /**
     * @brief Makes @p client the last Client in the given focus @p chain.
     *
     * This means the existing position of @p client is dropped and @p client is prepended to the
     * @p chain which makes it the last item.
     *
     * @param client The Client to become the last in @p chain
     * @param chain The focus chain to operate on
     * @return void
     */
    void makeLastInChain(Toplevel* window, Chain& chain);
    void moveAfterClientInChain(Toplevel* window, Toplevel* reference, Chain& chain);
    void updateClientInChain(Toplevel* window, Change change, Chain& chain);
    void insertClientIntoChain(Toplevel* window, Chain& chain);
    Chain m_mostRecentlyUsed;
    QHash<uint, Chain> m_desktopFocusChains;
    bool m_separateScreenFocus;
    Toplevel* m_activeClient;
    uint m_currentDesktop;

    KWIN_SINGLETON_VARIABLE(FocusChain, s_manager)
};

inline bool FocusChain::contains(Toplevel* window) const
{
    return m_mostRecentlyUsed.contains(window);
}

inline void FocusChain::setSeparateScreenFocus(bool enabled)
{
    m_separateScreenFocus = enabled;
}

inline void FocusChain::setActiveClient(Toplevel* window)
{
    m_activeClient = window;
}

inline void FocusChain::setCurrentDesktop(uint previous, uint newDesktop)
{
    Q_UNUSED(previous)
    m_currentDesktop = newDesktop;
}

} // KWin::win

} // KWin

#endif // KWIN_FOCUS_CHAIN_H