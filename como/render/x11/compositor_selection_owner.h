/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "base/x11/selection_owner.h"
#include "como_export.h"

#include <QObject>

namespace como::render::x11
{

class COMO_EXPORT compositor_selection_owner : public base::x11::selection_owner
{
    Q_OBJECT
public:
    compositor_selection_owner(char const* selection,
                               xcb_connection_t* con,
                               xcb_window_t root_window);

    bool is_owning() const;
    void own();
    void disown();

private:
    bool owning{false};
};

}
