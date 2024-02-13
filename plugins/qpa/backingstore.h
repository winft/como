/*
SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_QPA_BACKINGSTORE_H
#define KWIN_QPA_BACKINGSTORE_H

#include <epoxy/gl.h>

#include <qpa/qplatformbackingstore.h>

namespace como
{
namespace QPA
{

class BackingStore : public QPlatformBackingStore
{
public:
    explicit BackingStore(QWindow* window);
    ~BackingStore() override;

    QPaintDevice* paintDevice() override;
    void flush(QWindow* window, const QRegion& region, const QPoint& offset) override;
    void resize(const QSize& size, const QRegion& staticContents) override;

private:
    QImage m_buffer;
};

}
}

#endif
