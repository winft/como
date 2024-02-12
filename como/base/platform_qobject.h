/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como/base/output.h>
#include <como/base/output_topology.h>
#include <como_export.h>

#include <QObject>
#include <functional>

namespace como::base
{

class COMO_EXPORT platform_qobject : public QObject
{
    Q_OBJECT

public:
    platform_qobject(std::function<double()> get_scale)
        : get_scale{get_scale}
    {
    }

    std::function<double()> get_scale;

Q_SIGNALS:
    void output_added(como::base::output*);
    void output_removed(como::base::output*);
    void topology_changed(como::base::output_topology const& old_topo,
                          como::base::output_topology const& topo);

    // TODO(romangg): Either remove since it's only used in a test or find a better way to design
    //                the API. The current output is part of the output topology, but it shouldn't
    //                reuse the topology_changed signal, as this implies too much of a change.
    void current_output_changed(como::base::output const* old, como::base::output const* current);

    // Only relevant on Wayland with Xwayland being (re-)started later.
    // TODO(romangg): Move to Wayland platform?
    void x11_reset();
};

}
