/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"

#include <QObject>
#include <functional>

namespace como::input
{

class keyboard;
class pointer;
class switch_device;
class touch;

class COMO_EXPORT platform_qobject : public QObject
{
    Q_OBJECT
public:
    platform_qobject();
    ~platform_qobject() override;

Q_SIGNALS:
    void keyboard_added(como::input::keyboard*);
    void pointer_added(como::input::pointer*);
    void switch_added(como::input::switch_device*);
    void touch_added(como::input::touch*);

    void keyboard_removed(como::input::keyboard*);
    void pointer_removed(como::input::pointer*);
    void switch_removed(como::input::switch_device*);
    void touch_removed(como::input::touch*);
};

}
