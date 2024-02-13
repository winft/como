/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "base/x11/data.h"
#include "base/x11/xcb/property.h"
#include "utils/memory.h"
#include <render/x11/support_properties.h>

#include <render/effect/interface/effect.h>
#include <win/x11/unmanaged.h>
#include <win/x11/window_find.h>

#include <xcb/xcb.h>

namespace como::render::x11
{

template<typename Space>
EffectWindow* find_window_by_wid(Space& space, WId id)
{
    if (auto win = win::x11::find_controlled_window<typename Space::x11_window>(
            space, win::x11::predicate_match::window, id)) {
        return win->render->effect.get();
    }
    if (auto win = win::x11::find_unmanaged<typename Space::x11_window>(space, id)) {
        return win->render->effect.get();
    }
    return nullptr;
}

inline QByteArray read_window_property(xcb_connection_t* con,
                                       xcb_window_t win,
                                       xcb_atom_t atom,
                                       xcb_atom_t type,
                                       int format)
{
    if (win == XCB_WINDOW_NONE) {
        return QByteArray();
    }

    uint32_t len = 32768;

    for (;;) {
        base::x11::xcb::property prop(con, false, win, atom, XCB_ATOM_ANY, 0, len);
        if (prop.is_null()) {
            // get property failed
            return QByteArray();
        }

        if (prop->bytes_after > 0) {
            len *= 2;
            continue;
        }

        return prop.to_byte_array(format, type);
    }
}

template<typename Base>
QByteArray read_root_property(Base const& base, long atom, long type, int format)
{
    auto const& data = base.x11_data;
    if (!data.connection) {
        return QByteArray();
    }
    return read_window_property(data.connection, data.root_window, atom, type, format);
}

inline static xcb_atom_t register_support_property(base::x11::data const& data,
                                                   QByteArray const& name)
{
    auto c = data.connection;
    if (!c) {
        return XCB_ATOM_NONE;
    }

    // get the atom for the name
    unique_cptr<xcb_intern_atom_reply_t> atomReply(xcb_intern_atom_reply(
        c, xcb_intern_atom_unchecked(c, false, name.size(), name.constData()), nullptr));
    if (!atomReply) {
        return XCB_ATOM_NONE;
    }

    // announce property on root window
    unsigned char dummy = 0;
    xcb_change_property(
        c, XCB_PROP_MODE_REPLACE, data.root_window, atomReply->atom, atomReply->atom, 8, 1, &dummy);

    // TODO: add to _NET_SUPPORTED
    return atomReply->atom;
}

template<typename Effects>
void register_property_type(Effects& effects, long atom, bool reg)
{
    if (reg) {
        // initialized to 0 if not present yet
        ++effects.registered_atoms[atom];
    } else {
        if (--effects.registered_atoms[atom] == 0)
            effects.registered_atoms.remove(atom);
    }
}

template<typename Effects>
xcb_atom_t add_support_property(Effects& effects, QByteArray const& name)
{
    auto atom = register_support_property(effects.scene.platform.base.x11_data, name);
    if (atom == XCB_ATOM_NONE) {
        return atom;
    }

    keep_support_property(effects.scene.platform, atom);
    effects.m_managedProperties.insert(name, atom);
    register_property_type(effects, atom, true);

    return atom;
}

/**
 * @brief Announces support for the feature with the given name. If no other Effect
 * has announced support for this feature yet, an X11 property will be installed on
 * the root window.
 *
 * The Effect will be notified for events through the signal propertyNotify().
 *
 * To remove the support again use remove_support_property. When an Effect is
 * destroyed it is automatically taken care of removing the support. It is not
 * required to call remove_support_property in the Effect's cleanup handling.
 *
 * @param name The name of the property to announce support for
 * @param effect The effect which announces support
 * @return xcb_atom_t The created X11 atom
 */
template<typename Effects>
xcb_atom_t announce_support_property(Effects& effects, Effect* effect, QByteArray const& name)
{
    auto it = effects.m_propertiesForEffects.find(name);

    if (it != effects.m_propertiesForEffects.end()) {
        // Property already registered for an effect. Just append Effect and return the stored atom.
        if (!it.value().contains(effect)) {
            it.value().append(effect);
        }
        return effects.m_managedProperties.value(name, XCB_ATOM_NONE);
    }

    effects.m_propertiesForEffects.insert(name, QList<Effect*>() << effect);

    return add_support_property(effects, name);
}

/**
 * @brief Removes support for the feature with the given name. If there is no other Effect left
 * which has announced support for the given property, the property will be removed from the
 * root window.
 *
 * In case the Effect had not registered support, calling this function does not change
 * anything.
 *
 * @param name The name of the property to remove support for
 * @param effect The effect which had registered the property.
 */
template<typename Effects>
void remove_support_property(Effects& effects, Effect* effect, QByteArray const& name)
{
    auto it = effects.m_propertiesForEffects.find(name);
    if (it == effects.m_propertiesForEffects.end()) {
        // Property is not registered.
        return;
    }

    if (!it.value().contains(effect)) {
        // Property is not registered for given effect.
        return;
    }

    it.value().removeAll(effect);

    if (!it.value().isEmpty()) {
        // Property still registered for some effect. Nothing to clean up.
        return;
    }

    auto atom = effects.m_managedProperties.take(name);

    register_property_type(effects, atom, false);
    effects.m_propertiesForEffects.remove(name);

    // Delayed removal.
    remove_support_property(effects.scene.platform, atom);
}

}
