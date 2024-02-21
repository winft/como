/*
    SPDX-FileCopyrightText: 2004 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "book.h"

#include <como/base/logging.h>
#include <como/win/control.h>

#include "book_settings.h"
#include "rules_settings.h"

#include <KConfig>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace como::win::rules
{

book::book()
    : qobject{std::make_unique<book_qobject>()}
    , m_updateTimer(new QTimer(qobject.get()))
    , m_updatesDisabled(false)
{
    QObject::connect(m_updateTimer, &QTimer::timeout, qobject.get(), [this] { save(); });
    m_updateTimer->setInterval(1000);
    m_updateTimer->setSingleShot(true);
}

book::~book()
{
    save();
    deleteAll();
}

void book::deleteAll()
{
    qDeleteAll(m_rules);
    m_rules.clear();
}

void book::load()
{
    deleteAll();

    if (!config) {
        config = KSharedConfig::openConfig(QStringLiteral("kwinrulesrc"), KConfig::NoGlobals);
    } else {
        config->reparseConfiguration();
    }

    book_settings book(config);
    book.load();
    m_rules = book.rules();
}

void book::save()
{
    m_updateTimer->stop();

    if (!config) {
        qCWarning(KWIN_CORE) << "book::save invoked without prior invocation of book::load";
        return;
    }

    std::vector<ruling*> filteredRules;
    for (const auto& rule : std::as_const(m_rules)) {
        filteredRules.push_back(rule);
    }

    book_settings settings(config);
    settings.setRules(filteredRules);
    settings.save();
}

void book::requestDiskStorage()
{
    m_updateTimer->start();
}

void book::setUpdatesDisabled(bool disable)
{
    m_updatesDisabled = disable;
    if (!disable) {
        Q_EMIT qobject->updates_enabled();
    }
}

bool book::areUpdatesDisabled() const
{
    return m_updatesDisabled;
}

}
