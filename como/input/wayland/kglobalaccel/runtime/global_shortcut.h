/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <KGlobalShortcutInfo>

class GlobalShortcutContext;
class GlobalShortcutsRegistry;

/**
 * Represents a global shortcut.
 *
 * @internal
 *
 * \note This class can handle multiple keys (default and active). This
 * feature isn't used currently. kde4 only allows setting one key per global
 * shortcut.
 */
class GlobalShortcut
{
public:
    GlobalShortcut(GlobalShortcutsRegistry& registry,
                   QString const& uniqueName,
                   QString const& friendlyName,
                   GlobalShortcutContext* context);
    GlobalShortcut(GlobalShortcutsRegistry& registry);

    ~GlobalShortcut();

    //! Returns the context the shortcuts belongs to
    GlobalShortcutContext* context();
    GlobalShortcutContext const* context() const;

    //! Returns the default keys for this shortcut.
    QList<QKeySequence> defaultKeys() const;

    //! Return the friendly display name for this shortcut.
    QString friendlyName() const;

    //! Check if the shortcut is active. It's keys are grabbed
    bool isActive() const;

    //! Check if the shortcut is fresh/new. Is an internal state
    bool isFresh() const;

    //! Check if the shortcut is present. It application is running.
    bool isPresent() const;

    //! Returns true if the shortcut is a session shortcut
    bool isSessionShortcut() const;

    //! Returns a list of keys associated with this shortcut.
    QList<QKeySequence> keys() const;

    //! Activates the shortcut. The keys are grabbed.
    void setActive();

    //! Sets the default keys for this shortcut.
    void setDefaultKeys(QList<QKeySequence> const&);

    //! Sets the friendly name for the shortcut. For display.
    void setFriendlyName(QString const&);

    //! Sets the shortcut inactive. No longer grabs the keys.
    void setInactive();

    void setIsPresent(bool);
    void setIsFresh(bool);

    //! Sets the keys activated with this shortcut. The old keys are freed.
    void setKeys(QList<QKeySequence> const&);

    //! Returns the unique name aka id for the shortcuts.
    QString uniqueName() const;

    operator KGlobalShortcutInfo() const;

    //! Remove this shortcut and it's siblings
    void unRegister();

private:
    //! means the associated application is present.
    bool _isPresent : 1;

    //! means the shortcut is registered with GlobalShortcutsRegistry
    bool _isRegistered : 1;

    //! means the shortcut is new
    bool _isFresh : 1;

    GlobalShortcutsRegistry* _registry = nullptr;

    //! The context the shortcut belongs too
    GlobalShortcutContext* _context = nullptr;

    QString _uniqueName;
    QString _friendlyName; // usually localized

    QList<QKeySequence> _keys;
    QList<QKeySequence> _defaultKeys;
};
