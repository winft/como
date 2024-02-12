/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "como_export.h"

#include <QObject>

namespace KWin::win::deco
{

class COMO_EXPORT bridge_qobject : public QObject
{
    Q_OBJECT
Q_SIGNALS:
    void metaDataLoaded();
};

}
