/*
    SPDX-FileCopyrightText: 2023 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "catch_macros.h"

#include "como/base/wayland/app_singleton.h"
#include "helpers.h"

#include <KCrash>
#include <QApplication>
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    KCrash::setDrKonqiEnabled(false);
    KLocalizedString::setApplicationDomain("kwin");

    como::detail::test::prepare_app_env(argv[0]);

    como::base::wayland::app_singleton app(argc, argv);

    auto const own_path = app.qapp->libraryPaths().constLast();
    app.qapp->removeLibraryPath(own_path);
    app.qapp->addLibraryPath(own_path);

    return Catch::Session().run(argc, argv);
}
