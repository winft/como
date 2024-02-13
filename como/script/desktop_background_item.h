/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "window_thumbnail_item.h"

#include <como/base/output.h>
#include <como/win/subspace.h>

namespace como
{

/**
 * The desktop_background_item type is a convenience helper that represents the desktop
 * background on the specified screen, virtual desktop, and activity.
 */
class COMO_EXPORT desktop_background_item : public scripting::window_thumbnail_item
{
    Q_OBJECT
    Q_PROPERTY(QString outputName READ outputName WRITE setOutputName NOTIFY outputChanged)
    Q_PROPERTY(como::base::output* output READ output WRITE setOutput NOTIFY outputChanged)
    Q_PROPERTY(QString activity READ activity WRITE setActivity NOTIFY activityChanged)
    Q_PROPERTY(como::win::subspace* desktop READ desktop WRITE setDesktop NOTIFY desktopChanged)

public:
    explicit desktop_background_item(QQuickItem* parent = nullptr);

    void componentComplete() override;

    QString outputName() const;
    void setOutputName(QString const& name);

    base::output* output() const;
    void setOutput(base::output* output);

    win::subspace* desktop() const;
    void setDesktop(win::subspace* desktop);

    QString activity() const;
    void setActivity(QString const& activity);

Q_SIGNALS:
    void outputChanged();
    void desktopChanged();
    void activityChanged();

private:
    void updateWindow();

    base::output* m_output = nullptr;
    win::subspace* m_desktop = nullptr;
    QString m_activity;
};

}
