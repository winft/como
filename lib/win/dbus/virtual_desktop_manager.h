/*
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"
#include "virtual_desktop_types.h"
#include <win/subspace_manager.h>
#include <win/subspaces_get.h>
#include <win/subspaces_set.h>

#include <QObject>
#include <gsl/pointers>

namespace como::win
{

class subspace;
class subspace_manager_qobject;

namespace dbus
{

// TODO: disable all of this in case of kiosk?

class COMO_EXPORT subspace_manager_wrap : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.VirtualDesktopManager")

    /**
     * The number of virtual desktops currently available.
     * The ids of the virtual desktops are in the range [1,
     * win::subspace_manager::maximum()].
     */
    Q_PROPERTY(uint count READ count NOTIFY countChanged)
    /**
     * The number of rows the virtual desktops will be laid out in
     */
    Q_PROPERTY(uint rows READ rows WRITE setRows NOTIFY rowsChanged)
    /**
     * The id of the virtual desktop which is currently in use.
     */
    Q_PROPERTY(QString current READ current WRITE setCurrent NOTIFY currentChanged)
    /**
     * Whether navigation in the desktop layout wraps around at the borders.
     */
    Q_PROPERTY(bool navigationWrappingAround READ isNavigationWrappingAround WRITE
                   setNavigationWrappingAround NOTIFY navigationWrappingAroundChanged)

    /**
     * list of key/value pairs which every one of them is representing a desktop
     */
    Q_PROPERTY(como::win::dbus::subspace_data_vector desktops READ desktops NOTIFY desktopsChanged)

public:
    subspace_manager_wrap(win::subspace_manager_qobject* parent);
    ~subspace_manager_wrap() override = default;

    virtual uint count() const = 0;

    virtual void setRows(uint rows) = 0;
    virtual uint rows() const = 0;

    virtual void setCurrent(QString const& id) = 0;
    virtual QString current() const = 0;

    virtual void setNavigationWrappingAround(bool wraps) = 0;
    virtual bool isNavigationWrappingAround() const = 0;

    virtual subspace_data_vector desktops() const = 0;

Q_SIGNALS:
    void countChanged(uint count);
    void rowsChanged(uint rows);
    void currentChanged(QString const& id);
    void navigationWrappingAroundChanged(bool wraps);
    void desktopsChanged(como::win::dbus::subspace_data_vector);
    void desktopDataChanged(QString const& id, como::win::dbus::subspace_data);
    void desktopCreated(QString const& id, como::win::dbus::subspace_data);
    void desktopRemoved(QString const& id);

public:
    /// Create a desktop with a new name at a given position (starts from 1).
    virtual void createDesktop(uint position, QString const& name) = 0;
    virtual void setDesktopName(QString const& id, QString const& name) = 0;
    virtual void removeDesktop(QString const& id) = 0;

protected:
    void add_subspace(win::subspace& subspace);
    static subspace_data get_subspace_data(win::subspace& subspace);
};

template<typename Manager>
class subspace_manager : public subspace_manager_wrap
{
public:
    subspace_manager(Manager& manager)
        : subspace_manager_wrap(manager.qobject.get())
        , manager{&manager}
    {
        for (auto&& subspace : manager.subspaces) {
            add_subspace(*subspace);
        }
    }

    uint count() const override
    {
        return manager->subspaces.size();
    }

    void setRows(uint rows) override
    {
        if (static_cast<uint>(manager->grid.height()) == rows) {
            return;
        }

        subspace_manager_set_rows(*manager, rows);
        subspace_manager_save(*manager);
    }

    uint rows() const override
    {
        return manager->rows;
    }

    void setCurrent(QString const& id) override
    {
        if (manager->current->id() == id) {
            return;
        }

        if (auto sub = subspaces_get_for_id(*manager, id)) {
            subspaces_set_current(*manager, *sub);
        }
    }

    QString current() const override
    {
        return manager->current->id();
    }

    void setNavigationWrappingAround(bool wraps) override
    {
        subspace_manager_set_nav_wraps(*manager, wraps);
    }

    bool isNavigationWrappingAround() const override
    {
        return manager->nav_wraps;
    }

    subspace_data_vector desktops() const override
    {
        auto const& subs = manager->subspaces;
        subspace_data_vector vect;
        vect.reserve(manager->subspaces.size());

        std::transform(subs.cbegin(), subs.cend(), std::back_inserter(vect), [](auto sub) {
            return get_subspace_data(*sub);
        });

        return vect;
    }

    void createDesktop(uint position, QString const& name) override
    {
        subspace_manager_create_subspace(*manager, position, name);
    }

    void setDesktopName(QString const& id, QString const& name) override
    {
        auto sub = subspaces_get_for_id(*manager, id);
        if (!sub || sub->name() == name) {
            return;
        }

        sub->setName(name);
        subspace_manager_save(*manager);
    }

    void removeDesktop(QString const& id) override
    {
        if (auto sub = subspaces_get_for_id(*manager, id)) {
            subspace_manager_remove_subspace(*manager, sub);
        }
    }

private:
    gsl::not_null<Manager*> manager;
};

}
}
