/*
    SPDX-FileCopyrightText: 2009 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtual_desktops.h"

#include "input/redirect.h"
#include "main.h"

#include <KConfigGroup>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <NETWM>
#include <QAction>
#include <QUuid>
#include <Wrapland/Server/plasma_virtual_desktop.h>
#include <algorithm>

namespace KWin::win
{

static bool s_loadingDesktopSettings = false;

static QByteArray generateDesktopId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8();
}

virtual_desktop::virtual_desktop(QObject* parent)
    : QObject(parent)
{
}

virtual_desktop::~virtual_desktop()
{
    Q_EMIT aboutToBeDestroyed();
}

void virtual_desktop_manager::setVirtualDesktopManagement(
    Wrapland::Server::PlasmaVirtualDesktopManager* management)
{
    using namespace Wrapland::Server;
    Q_ASSERT(!m_virtualDesktopManagement);
    m_virtualDesktopManagement = management;

    auto createPlasmaVirtualDesktop = [this](auto desktop) {
        auto pvd = m_virtualDesktopManagement->createDesktop(desktop->id().toStdString(),
                                                             desktop->x11DesktopNumber() - 1);
        pvd->setName(desktop->name().toStdString());
        pvd->sendDone();

        connect(desktop, &virtual_desktop::nameChanged, pvd, [desktop, pvd] {
            pvd->setName(desktop->name().toStdString());
            pvd->sendDone();
        });
        connect(pvd, &PlasmaVirtualDesktop::activateRequested, this, [this, desktop] {
            setCurrent(desktop);
        });
    };

    connect(this,
            &virtual_desktop_manager::desktopCreated,
            m_virtualDesktopManagement,
            createPlasmaVirtualDesktop);

    connect(
        this, &virtual_desktop_manager::rowsChanged, m_virtualDesktopManagement, [this](uint rows) {
            m_virtualDesktopManagement->setRows(rows);
            m_virtualDesktopManagement->sendDone();
        });

    // handle removed: from virtual_desktop_manager to the wayland interface
    connect(this,
            &virtual_desktop_manager::desktopRemoved,
            m_virtualDesktopManagement,
            [this](auto desktop) {
                m_virtualDesktopManagement->removeDesktop(desktop->id().toStdString());
            });

    // create a new desktop when the client asks to
    connect(m_virtualDesktopManagement,
            &PlasmaVirtualDesktopManager::desktopCreateRequested,
            this,
            [this](auto const& name, quint32 position) {
                createVirtualDesktop(position, QString::fromStdString(name));
            });

    // remove when the client asks to
    connect(m_virtualDesktopManagement,
            &PlasmaVirtualDesktopManager::desktopRemoveRequested,
            this,
            [this](auto const& id) {
                // here there can be some nice kauthorized check?
                // remove only from virtual_desktop_manager, the other connections will remove it
                // from m_virtualDesktopManagement as well
                removeVirtualDesktop(id.c_str());
            });

    std::for_each(m_desktops.constBegin(), m_desktops.constEnd(), createPlasmaVirtualDesktop);

    // Now we are sure all ids are there
    save();

    connect(this, &virtual_desktop_manager::currentChanged, m_virtualDesktopManagement, [this]() {
        for (auto deskInt : m_virtualDesktopManagement->desktops()) {
            if (deskInt->id() == currentDesktop()->id().toStdString()) {
                deskInt->setActive(true);
            } else {
                deskInt->setActive(false);
            }
        }
    });
}

void virtual_desktop::setId(QByteArray const& id)
{
    Q_ASSERT(m_id.isEmpty());
    m_id = id;
}

void virtual_desktop::setX11DesktopNumber(uint number)
{
    // x11DesktopNumber can be changed now
    if (static_cast<uint>(m_x11DesktopNumber) == number) {
        return;
    }

    m_x11DesktopNumber = number;

    if (m_x11DesktopNumber != 0) {
        Q_EMIT x11DesktopNumberChanged();
    }
}

void virtual_desktop::setName(QString const& name)
{
    if (m_name == name) {
        return;
    }

    m_name = name;
    Q_EMIT nameChanged();
}

virtual_desktop_grid::virtual_desktop_grid()
    : m_size(1, 2) // Default to tow rows
    , m_grid(QVector<QVector<virtual_desktop*>>{QVector<virtual_desktop*>{},
                                                QVector<virtual_desktop*>{}})
{
}

virtual_desktop_grid::~virtual_desktop_grid() = default;

void virtual_desktop_grid::update(QSize const& size,
                                  Qt::Orientation orientation,
                                  QVector<virtual_desktop*> const& desktops)
{
    // Set private variables
    m_size = size;
    uint const width = size.width();
    uint const height = size.height();

    m_grid.clear();
    auto it = desktops.begin();
    auto end = desktops.end();

    if (orientation == Qt::Horizontal) {
        for (uint y = 0; y < height; ++y) {
            QVector<virtual_desktop*> row;
            for (uint x = 0; x < width && it != end; ++x) {
                row << *it;
                it++;
            }
            m_grid << row;
        }
    } else {
        for (uint y = 0; y < height; ++y) {
            m_grid << QVector<virtual_desktop*>();
        }
        for (uint x = 0; x < width; ++x) {
            for (uint y = 0; y < height && it != end; ++y) {
                auto& row = m_grid[y];
                row << *it;
                it++;
            }
        }
    }
}

QPoint virtual_desktop_grid::gridCoords(uint id) const
{
    return gridCoords(virtual_desktop_manager::self()->desktopForX11Id(id));
}

QPoint virtual_desktop_grid::gridCoords(virtual_desktop* vd) const
{
    for (int y = 0; y < m_grid.count(); ++y) {
        auto const& row = m_grid.at(y);
        for (int x = 0; x < row.count(); ++x) {
            if (row.at(x) == vd) {
                return QPoint(x, y);
            }
        }
    }

    return QPoint(-1, -1);
}

virtual_desktop* virtual_desktop_grid::at(const QPoint& coords) const
{
    if (coords.y() >= m_grid.count()) {
        return nullptr;
    }

    auto const& row = m_grid.at(coords.y());
    if (coords.x() >= row.count()) {
        return nullptr;
    }

    return row.at(coords.x());
}

KWIN_SINGLETON_FACTORY_VARIABLE(virtual_desktop_manager, s_manager)

virtual_desktop_manager::virtual_desktop_manager(QObject* parent)
    : QObject(parent)
    , m_navigationWrapsAround(false)
    , m_rootInfo(nullptr)
{
}

virtual_desktop_manager::~virtual_desktop_manager()
{
    s_manager = nullptr;
}

void virtual_desktop_manager::setRootInfo(NETRootInfo* info)
{
    m_rootInfo = info;

    // Nothing will be connected to rootInfo
    if (m_rootInfo) {
        int columns = count() / m_rows;
        if (count() % m_rows > 0) {
            columns++;
        }

        m_rootInfo->setDesktopLayout(
            NET::OrientationHorizontal, columns, m_rows, NET::DesktopLayoutCornerTopLeft);
        updateRootInfo();
        m_rootInfo->setCurrentDesktop(currentDesktop()->x11DesktopNumber());

        for (auto vd : m_desktops) {
            m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
        }
    }
}

QString virtual_desktop_manager::name(uint desktop) const
{
    if (uint(m_desktops.length()) > desktop - 1) {
        return m_desktops[desktop - 1]->name();
    }

    if (!m_rootInfo) {
        return defaultName(desktop);
    }
    return QString::fromUtf8(m_rootInfo->desktopName(desktop));
}

uint virtual_desktop_manager::above(uint id, bool wrap) const
{
    auto vd = above(desktopForX11Id(id), wrap);
    return vd ? vd->x11DesktopNumber() : 0;
}

virtual_desktop* virtual_desktop_manager::above(virtual_desktop* desktop, bool wrap) const
{
    Q_ASSERT(m_current);

    if (!desktop) {
        desktop = m_current;
    }

    QPoint coords = m_grid.gridCoords(desktop);
    Q_ASSERT(coords.x() >= 0);

    while (true) {
        coords.ry()--;
        if (coords.y() < 0) {
            if (wrap) {
                coords.setY(m_grid.height() - 1);
            } else {
                // Already at the top-most desktop
                return desktop;
            }
        }

        if (auto vd = m_grid.at(coords)) {
            return vd;
        }
    }

    return nullptr;
}

uint virtual_desktop_manager::toRight(uint id, bool wrap) const
{
    auto vd = toRight(desktopForX11Id(id), wrap);
    return vd ? vd->x11DesktopNumber() : 0;
}

virtual_desktop* virtual_desktop_manager::toRight(virtual_desktop* desktop, bool wrap) const
{
    Q_ASSERT(m_current);

    if (!desktop) {
        desktop = m_current;
    }

    QPoint coords = m_grid.gridCoords(desktop);
    Q_ASSERT(coords.x() >= 0);

    while (true) {
        coords.rx()++;
        if (coords.x() >= m_grid.width()) {
            if (wrap) {
                coords.setX(0);
            } else {
                // Already at the right-most desktop
                return desktop;
            }
        }

        if (auto vd = m_grid.at(coords)) {
            return vd;
        }
    }

    return nullptr;
}

uint virtual_desktop_manager::below(uint id, bool wrap) const
{
    auto vd = below(desktopForX11Id(id), wrap);
    return vd ? vd->x11DesktopNumber() : 0;
}

virtual_desktop* virtual_desktop_manager::below(virtual_desktop* desktop, bool wrap) const
{
    Q_ASSERT(m_current);

    if (!desktop) {
        desktop = m_current;
    }

    QPoint coords = m_grid.gridCoords(desktop);
    Q_ASSERT(coords.x() >= 0);

    while (true) {
        coords.ry()++;
        if (coords.y() >= m_grid.height()) {
            if (wrap) {
                coords.setY(0);
            } else {
                // Already at the bottom-most desktop
                return desktop;
            }
        }

        if (auto vd = m_grid.at(coords)) {
            return vd;
        }
    }

    return nullptr;
}

uint virtual_desktop_manager::toLeft(uint id, bool wrap) const
{
    auto vd = toLeft(desktopForX11Id(id), wrap);
    return vd ? vd->x11DesktopNumber() : 0;
}

virtual_desktop* virtual_desktop_manager::toLeft(virtual_desktop* desktop, bool wrap) const
{
    Q_ASSERT(m_current);

    if (!desktop) {
        desktop = m_current;
    }

    QPoint coords = m_grid.gridCoords(desktop);
    Q_ASSERT(coords.x() >= 0);

    while (true) {
        coords.rx()--;
        if (coords.x() < 0) {
            if (wrap) {
                coords.setX(m_grid.width() - 1);
            } else {
                return desktop; // Already at the left-most desktop
            }
        }

        if (auto vd = m_grid.at(coords)) {
            return vd;
        }
    }

    return nullptr;
}

virtual_desktop* virtual_desktop_manager::next(virtual_desktop* desktop, bool wrap) const
{
    Q_ASSERT(m_current);
    if (!desktop) {
        desktop = m_current;
    }

    auto it = std::find(m_desktops.begin(), m_desktops.end(), desktop);
    Q_ASSERT(it != m_desktops.end());
    it++;

    if (it == m_desktops.end()) {
        if (wrap) {
            return m_desktops.first();
        } else {
            return desktop;
        }
    }

    return *it;
}

virtual_desktop* virtual_desktop_manager::previous(virtual_desktop* desktop, bool wrap) const
{
    Q_ASSERT(m_current);
    if (!desktop) {
        desktop = m_current;
    }

    auto it = std::find(m_desktops.begin(), m_desktops.end(), desktop);
    Q_ASSERT(it != m_desktops.end());

    if (it == m_desktops.begin()) {
        if (wrap) {
            return m_desktops.last();
        } else {
            return desktop;
        }
    }

    it--;
    return *it;
}

virtual_desktop* virtual_desktop_manager::desktopForX11Id(uint id) const
{
    if (id == 0 || id > count()) {
        return nullptr;
    }
    return m_desktops.at(id - 1);
}

virtual_desktop* virtual_desktop_manager::desktopForId(QByteArray const& id) const
{
    auto desk = std::find_if(m_desktops.constBegin(), m_desktops.constEnd(), [id](auto desk) {
        return desk->id() == id;
    });

    if (desk != m_desktops.constEnd()) {
        return *desk;
    }

    return nullptr;
}

virtual_desktop* virtual_desktop_manager::createVirtualDesktop(uint position, QString const& name)
{
    // too many, can't insert new ones
    if ((uint)m_desktops.count() == virtual_desktop_manager::maximum()) {
        return nullptr;
    }

    position = qBound(0u, position, static_cast<uint>(m_desktops.count()));

    QString desktopName = name;
    if (desktopName.isEmpty()) {
        desktopName = defaultName(position + 1);
    }

    auto vd = new virtual_desktop(this);
    vd->setX11DesktopNumber(position + 1);
    vd->setId(generateDesktopId());
    vd->setName(desktopName);

    connect(vd, &virtual_desktop::nameChanged, this, [this, vd]() {
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
        }
    });

    if (m_rootInfo) {
        m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
    }

    m_desktops.insert(position, vd);

    // update the id of displaced desktops
    for (uint i = position + 1; i < (uint)m_desktops.count(); ++i) {
        m_desktops[i]->setX11DesktopNumber(i + 1);
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(i + 1, m_desktops[i]->name().toUtf8().data());
        }
    }

    save();
    updateRootInfo();

    Q_EMIT desktopCreated(vd);
    Q_EMIT countChanged(m_desktops.count() - 1, m_desktops.count());

    return vd;
}

void virtual_desktop_manager::removeVirtualDesktop(QByteArray const& id)
{
    auto desktop = desktopForId(id);
    if (desktop) {
        removeVirtualDesktop(desktop);
    }
}

void virtual_desktop_manager::removeVirtualDesktop(virtual_desktop* desktop)
{
    assert(desktop);

    // don't end up without any desktop
    if (m_desktops.count() == 1) {
        return;
    }

    uint const oldCurrent = m_current->x11DesktopNumber();
    uint const i = desktop->x11DesktopNumber() - 1;
    m_desktops.remove(i);

    for (uint j = i; j < (uint)m_desktops.count(); ++j) {
        m_desktops[j]->setX11DesktopNumber(j + 1);
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(j + 1, m_desktops[j]->name().toUtf8().data());
        }
    }

    uint const newCurrent = qMin(oldCurrent, (uint)m_desktops.count());
    m_current = m_desktops.at(newCurrent - 1);
    if (oldCurrent != newCurrent) {
        Q_EMIT currentChanged(oldCurrent, newCurrent);
    }

    save();

    updateRootInfo();
    Q_EMIT desktopRemoved(desktop);
    Q_EMIT countChanged(m_desktops.count() + 1, m_desktops.count());

    desktop->deleteLater();
}

uint virtual_desktop_manager::current() const
{
    return m_current ? m_current->x11DesktopNumber() : 0;
}

virtual_desktop* virtual_desktop_manager::currentDesktop() const
{
    return m_current;
}

bool virtual_desktop_manager::setCurrent(uint newDesktop)
{
    if (newDesktop < 1 || newDesktop > count() || newDesktop == current()) {
        return false;
    }

    auto d = desktopForX11Id(newDesktop);
    Q_ASSERT(d);
    return setCurrent(d);
}

bool virtual_desktop_manager::setCurrent(virtual_desktop* newDesktop)
{
    Q_ASSERT(newDesktop);
    if (m_current == newDesktop) {
        return false;
    }

    uint const oldDesktop = current();
    m_current = newDesktop;

    Q_EMIT currentChanged(oldDesktop, newDesktop->x11DesktopNumber());
    return true;
}

void virtual_desktop_manager::setCount(uint count)
{
    count = qBound<uint>(1, count, virtual_desktop_manager::maximum());
    if (count == uint(m_desktops.count())) {
        // nothing to change
        return;
    }

    QList<virtual_desktop*> newDesktops;
    uint const oldCount = m_desktops.count();

    // this explicit check makes it more readable
    if ((uint)m_desktops.count() > count) {
        auto const desktopsToRemove = m_desktops.mid(count);
        m_desktops.resize(count);
        if (m_current) {
            uint oldCurrent = current();
            uint newCurrent = qMin(oldCurrent, count);
            m_current = m_desktops.at(newCurrent - 1);
            if (oldCurrent != newCurrent) {
                Q_EMIT currentChanged(oldCurrent, newCurrent);
            }
        }
        for (auto desktop : desktopsToRemove) {
            Q_EMIT desktopRemoved(desktop);
            desktop->deleteLater();
        }
    } else {
        while (uint(m_desktops.count()) < count) {
            auto vd = new virtual_desktop(this);
            const int x11Number = m_desktops.count() + 1;
            vd->setX11DesktopNumber(x11Number);
            vd->setName(defaultName(x11Number));
            if (!s_loadingDesktopSettings) {
                vd->setId(generateDesktopId());
            }
            m_desktops << vd;
            newDesktops << vd;
            connect(vd, &virtual_desktop::nameChanged, this, [this, vd] {
                if (m_rootInfo) {
                    m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
                }
            });
            if (m_rootInfo) {
                m_rootInfo->setDesktopName(vd->x11DesktopNumber(), vd->name().toUtf8().data());
            }
        }
    }

    updateRootInfo();

    if (!s_loadingDesktopSettings) {
        save();
    }
    for (auto vd : newDesktops) {
        Q_EMIT desktopCreated(vd);
    }

    Q_EMIT countChanged(oldCount, m_desktops.count());
}

uint virtual_desktop_manager::rows() const
{
    return m_rows;
}

void virtual_desktop_manager::setRows(uint rows)
{
    if (rows == 0 || rows > count() || rows == m_rows) {
        return;
    }

    m_rows = rows;
    int columns = count() / m_rows;

    if (count() % m_rows > 0) {
        columns++;
    }

    if (m_rootInfo) {
        m_rootInfo->setDesktopLayout(
            NET::OrientationHorizontal, columns, m_rows, NET::DesktopLayoutCornerTopLeft);
        m_rootInfo->activate();
    }

    updateLayout();

    // rowsChanged will be emitted by setNETDesktopLayout called by updateLayout
}

void virtual_desktop_manager::updateRootInfo()
{
    if (!m_rootInfo) {
        // Make sure the layout is still valid
        updateLayout();
        return;
    }

    const int n = count();
    m_rootInfo->setNumberOfDesktops(n);

    NETPoint* viewports = new NETPoint[n];
    m_rootInfo->setDesktopViewport(n, *viewports);
    delete[] viewports;

    // Make sure the layout is still valid
    updateLayout();
}

void virtual_desktop_manager::updateLayout()
{
    m_rows = qMin(m_rows, count());

    int columns = count() / m_rows;
    Qt::Orientation orientation = Qt::Horizontal;

    if (m_rootInfo) {
        // TODO: Is there a sane way to avoid overriding the existing grid?
        columns = m_rootInfo->desktopLayoutColumnsRows().width();
        m_rows = qMax(1, m_rootInfo->desktopLayoutColumnsRows().height());
        orientation = m_rootInfo->desktopLayoutOrientation() == NET::OrientationHorizontal
            ? Qt::Horizontal
            : Qt::Vertical;
    }

    if (columns == 0) {
        // Not given, set default layout
        m_rows = count() == 1u ? 1 : 2;
        columns = count() / m_rows;
    }

    setNETDesktopLayout(
        orientation,
        columns,
        m_rows,
        0 // rootInfo->desktopLayoutCorner() // Not really worth implementing right now.
    );
}

void virtual_desktop_manager::load()
{
    s_loadingDesktopSettings = true;

    if (!m_config) {
        return;
    }

    KConfigGroup group(m_config, QStringLiteral("Desktops"));
    const int n = group.readEntry("Number", 1);
    setCount(n);

    for (int i = 1; i <= n; i++) {
        QString s = group.readEntry(QStringLiteral("Name_%1").arg(i), i18n("Desktop %1", i));
        if (m_rootInfo) {
            m_rootInfo->setDesktopName(i, s.toUtf8().data());
        }
        m_desktops[i - 1]->setName(s.toUtf8().data());

        auto const sId = group.readEntry(QStringLiteral("Id_%1").arg(i), QString());

        if (m_desktops[i - 1]->id().isEmpty()) {
            m_desktops[i - 1]->setId(sId.isEmpty() ? generateDesktopId() : sId.toUtf8());
        } else {
            Q_ASSERT(sId.isEmpty() || m_desktops[i - 1]->id() == sId.toUtf8().data());
        }

        // TODO: update desktop focus chain, why?
        //         m_desktopFocusChain.value()[i-1] = i;
    }

    int rows = group.readEntry<int>("Rows", 2);
    m_rows = qBound(1, rows, n);

    s_loadingDesktopSettings = false;
}

void virtual_desktop_manager::save()
{
    if (s_loadingDesktopSettings) {
        return;
    }
    if (!m_config) {
        return;
    }
    KConfigGroup group(m_config, QStringLiteral("Desktops"));

    for (int i = count() + 1; group.hasKey(QStringLiteral("Id_%1").arg(i)); i++) {
        group.deleteEntry(QStringLiteral("Id_%1").arg(i));
        group.deleteEntry(QStringLiteral("Name_%1").arg(i));
    }

    group.writeEntry("Number", count());

    for (uint i = 1; i <= count(); ++i) {
        QString s = name(i);
        auto const defaultvalue = defaultName(i);

        if (s.isEmpty()) {
            s = defaultvalue;
            if (m_rootInfo) {
                m_rootInfo->setDesktopName(i, s.toUtf8().data());
            }
        }

        if (s != defaultvalue) {
            group.writeEntry(QStringLiteral("Name_%1").arg(i), s);
        } else {
            QString currentvalue = group.readEntry(QStringLiteral("Name_%1").arg(i), QString());
            if (currentvalue != defaultvalue) {
                group.deleteEntry(QStringLiteral("Name_%1").arg(i));
            }
        }
        group.writeEntry(QStringLiteral("Id_%1").arg(i), m_desktops[i - 1]->id());
    }

    group.writeEntry("Rows", m_rows);

    // Save to disk
    group.sync();
}

QString virtual_desktop_manager::defaultName(int desktop) const
{
    return i18n("Desktop %1", desktop);
}

void virtual_desktop_manager::setNETDesktopLayout(Qt::Orientation orientation,
                                                  uint width,
                                                  uint height,
                                                  int startingCorner)
{
    Q_UNUSED(startingCorner); // Not really worth implementing right now.
    uint const count = m_desktops.count();

    // Calculate valid grid size
    Q_ASSERT(width > 0 || height > 0);

    if ((width <= 0) && (height > 0)) {
        width = (count + height - 1) / height;
    } else if ((height <= 0) && (width > 0)) {
        height = (count + width - 1) / width;
    }

    while (width * height < count) {
        if (orientation == Qt::Horizontal) {
            ++width;
        } else {
            ++height;
        }
    }

    m_rows = qMax(1u, height);
    m_grid.update(QSize(width, height), orientation, m_desktops);

    // TODO: why is there no call to m_rootInfo->setDesktopLayout?
    Q_EMIT layoutChanged(width, height);
    Q_EMIT rowsChanged(height);
}

void virtual_desktop_manager::initShortcuts()
{
    initSwitchToShortcuts();

    auto nextAction = addAction(QStringLiteral("Switch to Next Desktop"),
                                i18n("Switch to Next Desktop"),
                                &virtual_desktop_manager::slotNext);
    kwinApp()->input->redirect->registerTouchpadSwipeShortcut(SwipeDirection::Right, nextAction);

    auto previousAction = addAction(QStringLiteral("Switch to Previous Desktop"),
                                    i18n("Switch to Previous Desktop"),
                                    &virtual_desktop_manager::slotPrevious);
    kwinApp()->input->redirect->registerTouchpadSwipeShortcut(SwipeDirection::Left, previousAction);

    addAction(QStringLiteral("Switch One Desktop to the Right"),
              i18n("Switch One Desktop to the Right"),
              &virtual_desktop_manager::slotRight);
    addAction(QStringLiteral("Switch One Desktop to the Left"),
              i18n("Switch One Desktop to the Left"),
              &virtual_desktop_manager::slotLeft);
    addAction(QStringLiteral("Switch One Desktop Up"),
              i18n("Switch One Desktop Up"),
              &virtual_desktop_manager::slotUp);
    addAction(QStringLiteral("Switch One Desktop Down"),
              i18n("Switch One Desktop Down"),
              &virtual_desktop_manager::slotDown);

    // axis events
    kwinApp()->input->redirect->registerAxisShortcut(
        Qt::ControlModifier | Qt::AltModifier,
        PointerAxisDown,
        findChild<QAction*>(QStringLiteral("Switch to Next Desktop")));
    kwinApp()->input->redirect->registerAxisShortcut(
        Qt::ControlModifier | Qt::AltModifier,
        PointerAxisUp,
        findChild<QAction*>(QStringLiteral("Switch to Previous Desktop")));
}

void virtual_desktop_manager::initSwitchToShortcuts()
{
    auto const toDesktop = QStringLiteral("Switch to Desktop %1");
    KLocalizedString const toDesktopLabel = ki18n("Switch to Desktop %1");

    addAction(toDesktop,
              toDesktopLabel,
              1,
              QKeySequence(Qt::CTRL + Qt::Key_F1),
              &virtual_desktop_manager::slotSwitchTo);
    addAction(toDesktop,
              toDesktopLabel,
              2,
              QKeySequence(Qt::CTRL + Qt::Key_F2),
              &virtual_desktop_manager::slotSwitchTo);
    addAction(toDesktop,
              toDesktopLabel,
              3,
              QKeySequence(Qt::CTRL + Qt::Key_F3),
              &virtual_desktop_manager::slotSwitchTo);
    addAction(toDesktop,
              toDesktopLabel,
              4,
              QKeySequence(Qt::CTRL + Qt::Key_F4),
              &virtual_desktop_manager::slotSwitchTo);

    for (uint i = 5; i <= maximum(); ++i) {
        addAction(
            toDesktop, toDesktopLabel, i, QKeySequence(), &virtual_desktop_manager::slotSwitchTo);
    }
}

QAction* virtual_desktop_manager::addAction(QString const& name,
                                            KLocalizedString const& label,
                                            uint value,
                                            const QKeySequence& key,
                                            void (virtual_desktop_manager::*slot)())
{
    auto a = new QAction(this);
    a->setProperty("componentName", QStringLiteral(KWIN_NAME));
    a->setObjectName(name.arg(value));
    a->setText(label.subs(value).toString());
    a->setData(value);

    KGlobalAccel::setGlobalShortcut(a, key);
    kwinApp()->input->redirect->registerShortcut(key, a, this, slot);

    return a;
}

QAction* virtual_desktop_manager::addAction(QString const& name,
                                            QString const& label,
                                            void (virtual_desktop_manager::*slot)())
{
    auto a = new QAction(this);
    a->setProperty("componentName", QStringLiteral(KWIN_NAME));
    a->setObjectName(name);
    a->setText(label);

    KGlobalAccel::setGlobalShortcut(a, QKeySequence());
    kwinApp()->input->redirect->registerShortcut(QKeySequence(), a, this, slot);

    return a;
}

void virtual_desktop_manager::slotSwitchTo()
{
    auto act = qobject_cast<QAction*>(sender());
    if (!act) {
        return;
    }

    bool ok = false;
    uint const i = act->data().toUInt(&ok);
    if (!ok) {
        return;
    }

    setCurrent(i);
}

void virtual_desktop_manager::setNavigationWrappingAround(bool enabled)
{
    if (enabled == m_navigationWrapsAround) {
        return;
    }

    m_navigationWrapsAround = enabled;
    Q_EMIT navigationWrappingAroundChanged();
}

void virtual_desktop_manager::slotDown()
{
    moveTo<virtual_desktop_below>(isNavigationWrappingAround());
}

void virtual_desktop_manager::slotLeft()
{
    moveTo<virtual_desktop_left>(isNavigationWrappingAround());
}

void virtual_desktop_manager::slotPrevious()
{
    moveTo<virtual_desktop_previous>(isNavigationWrappingAround());
}

void virtual_desktop_manager::slotNext()
{
    moveTo<virtual_desktop_next>(isNavigationWrappingAround());
}

void virtual_desktop_manager::slotRight()
{
    moveTo<virtual_desktop_right>(isNavigationWrappingAround());
}

void virtual_desktop_manager::slotUp()
{
    moveTo<virtual_desktop_above>(isNavigationWrappingAround());
}

}