/*
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "filtered_display.h"

#include "base/logging.h"
#include "desktop/kde/service_utils.h"

#include <QCryptographicHash>
#include <QFile>
#include <Wrapland/Server/client.h>
#include <unistd.h>

namespace como::base::wayland
{

QSet<QByteArray> const interfacesBlackList = {
    "org_kde_kwin_remote_access_manager",
    "org_kde_plasma_window_management",
    "org_kde_kwin_fake_input",
    "org_kde_kwin_keystate",
};

QByteArray sha256(QString const& fileName)
{
    QFile f(fileName);
    if (f.open(QFile::ReadOnly)) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (hash.addData(&f)) {
            return hash.result();
        }
    }
    return QByteArray();
}

bool is_trusted_origin(Wrapland::Server::Client* client)
{
    auto fullPathSha = sha256(QString::fromStdString(client->executablePath()));
    auto localSha = sha256(QLatin1String("/proc/") + QString::number(client->processId())
                           + QLatin1String("/exe"));
    auto trusted = !localSha.isEmpty() && fullPathSha == localSha;

    if (!trusted) {
        qCWarning(KWIN_CORE) << "Could not trust" << client->executablePath().c_str() << "sha"
                             << localSha << fullPathSha;
    }

    return trusted;
}

QStringList fetch_requested_interfaces(Wrapland::Server::Client* client)
{
    if (!client->security_context_app_id().empty()) {
        return desktop::kde::fetchRequestedInterfacesForDesktopId(
            client->security_context_app_id().c_str());
    }
    return desktop::kde::fetchRequestedInterfaces(client->executablePath().c_str());
}

filtered_display::filtered_display()
    : Wrapland::Server::FilteredDisplay()
{
}

bool filtered_display::allowInterface(Wrapland::Server::Client* client,
                                      QByteArray const& interfaceName)
{
    if (!client->security_context_app_id().empty()
        && interfaceName == QByteArrayLiteral("wp_security_context_manager_v1")) {
        return false;
    }
    if (client->processId() == getpid()) {
        return true;
    }

    if (!interfacesBlackList.contains(interfaceName)) {
        return true;
    }

    if (client->executablePath().empty()) {
        qCDebug(KWIN_CORE) << "Could not identify process with pid" << client->processId();
        return false;
    }

    auto requestedInterfaces = client->property("requestedInterfaces");
    if (requestedInterfaces.isNull()) {
        requestedInterfaces = fetch_requested_interfaces(client);
        client->setProperty("requestedInterfaces", requestedInterfaces);
    }

    if (!requestedInterfaces.toStringList().contains(QString::fromUtf8(interfaceName))) {
        if (KWIN_CORE().isDebugEnabled()) {
            QString const id = QString::fromStdString(client->executablePath()) + QLatin1Char('|')
                + QString::fromUtf8(interfaceName);
            if (!reported.contains({id})) {
                reported.insert(id);
                qCDebug(KWIN_CORE)
                    << "Interface" << interfaceName << "not in X-KDE-Wayland-Interfaces of"
                    << client->executablePath().c_str();
            }
        }
        return false;
    }

    auto trustedOrigin = client->property("isPrivileged");
    if (trustedOrigin.isNull()) {
        trustedOrigin = is_trusted_origin(client);
        client->setProperty("isPrivileged", trustedOrigin);
    }

    if (!trustedOrigin.toBool()) {
        return false;
    }

    qCDebug(KWIN_CORE) << "authorized" << client->executablePath().c_str() << interfaceName;
    return true;
}

}
