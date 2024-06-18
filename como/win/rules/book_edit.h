/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "config-como.h"
#include <como/base/logging.h>

#include <QFileInfo>
#include <QProcess>

namespace como::win::rules
{

template<typename Book, typename RefWin>
void discard_used_rules(Book& book, RefWin& ref_win, bool withdrawn)
{
    for (auto it = book.m_rules.begin(); it != book.m_rules.end();) {
        if (ref_win.control->rules.contains(*it)) {
            auto const index = book.settings->indexForId((*it)->id);

            if ((*it)->discardUsed(withdrawn)) {
                if (index) {
                    auto settings = book.settings->ruleSettingsAt(index.value());
                    (*it)->write(settings);
                }
            }
            if ((*it)->isEmpty()) {
                ref_win.control->remove_rule(*it);
                auto r = *it;
                it = book.m_rules.erase(it);
                delete r;
                if (index) {
                    book.settings->removeRuleSettingsAt(index.value());
                }
                continue;
            }
        }
        ++it;
    }

    if (book.settings->usrIsSaveNeeded()) {
        book.requestDiskStorage();
    }
}

template<typename Book, typename RefWin>
void edit_book(Book& book, RefWin& ref_win, bool whole_app)
{
    book.save();

    QStringList args;
    args << QStringLiteral("uuid=%1").arg(ref_win.meta.internal_id.toString());

    if (whole_app) {
        args << QStringLiteral("whole-app");
    }

    auto p = new QProcess(book.qobject.get());
    p->setArguments({"kcm_kwinrules", "--args", args.join(QLatin1Char(' '))});

    if constexpr (requires(decltype(ref_win.space.base) base) { base.process_environment; }) {
        p->setProcessEnvironment(ref_win.space.base.process_environment);
    }

    p->setProgram(QStandardPaths::findExecutable("kcmshell6"));
    p->setProcessChannelMode(QProcess::MergedChannels);

    QObject::connect(
        p,
        static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
        p,
        &QProcess::deleteLater);
    QObject::connect(
        p, &QProcess::errorOccurred, book.qobject.get(), [p](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
                qCDebug(KWIN_CORE) << "Failed to start" << p->program();
            }
        });

    p->start();
}

}
