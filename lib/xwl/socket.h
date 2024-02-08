/*
SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <como_export.h>

#include <string>
#include <vector>

namespace como::xwl
{

class COMO_EXPORT socket
{
public:
    enum class mode {
        close_fds_on_exec,
        transfer_fds_on_exec,
    };

    socket(socket::mode mode);
    ~socket();

    bool is_valid() const
    {
        return display != -1;
    }

    std::string name() const
    {
        return ":" + std::to_string(display);
    }

    int display = -1;
    std::vector<int> file_descriptors;

private:
    std::string socket_file_path;
    std::string lock_file_path;
};

}
