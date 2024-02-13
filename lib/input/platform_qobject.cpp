/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "platform_qobject.h"

#include "singleton_interface.h"

namespace como::input
{

platform_qobject::platform_qobject()
{
    singleton_interface::platform_qobject = this;
}

platform_qobject::~platform_qobject()
{
    singleton_interface::platform_qobject = nullptr;
}

}
