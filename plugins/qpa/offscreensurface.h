/*
SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <qpa/qplatformoffscreensurface.h>

namespace como
{
namespace QPA
{

class OffscreenSurface : public QPlatformOffscreenSurface
{
public:
    explicit OffscreenSurface(QOffscreenSurface* surface);

    QSurfaceFormat format() const override;
    bool isValid() const override;

private:
    QSurfaceFormat m_format;
};

} // namespace QPA
}
