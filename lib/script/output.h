/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"

#include <QObject>

namespace KWin::scripting
{

class COMO_EXPORT output : public QObject
{
    Q_OBJECT
public:
    output();
};

template<typename RefOut>
class output_impl : public output
{
public:
    output_impl(RefOut& ref_out)
        : ref_out{ref_out}
    {
    }

    RefOut& ref_out;
};

}

Q_DECLARE_METATYPE(KWin::scripting::output*)
