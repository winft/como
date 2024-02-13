/*
    SPDX-FileCopyrightText: 2021 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "../console.h"

namespace como::debug
{

template<typename Space>
class x11_console : public console<Space>
{
public:
    explicit x11_console(Space& space)
        : console<Space>(space)
    {
        this->m_ui->windowsView->setItemDelegate(new console_delegate(this));

        auto model = new console_model(this);
        model_setup_connections(*model, space);
        this->m_ui->windowsView->setModel(model);

        this->m_ui->tabWidget->setTabEnabled(1, false);
        this->m_ui->tabWidget->setTabEnabled(2, false);
        this->m_ui->tabWidget->setTabEnabled(3, false);
        this->m_ui->tabWidget->setTabEnabled(5, false);

        // for X11
        this->setWindowFlags(Qt::X11BypassWindowManagerHint);
    }
};

}
