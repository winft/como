/*
SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QLoggingCategory>
#include <QtGui/private/qtx11extras_p.h>
#include <QtTest>

#include "../testutils.h"
#include "render/xrender/utils.h"

class BlendPictureTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testDontCrashOnTeardown();
};

void BlendPictureTest::initTestCase()
{
    como::XRenderUtils::init(QX11Info::connection(), QX11Info::appRootWindow());
}

void BlendPictureTest::cleanupTestCase()
{
    como::XRenderUtils::cleanup();
}

void BlendPictureTest::testDontCrashOnTeardown()
{
    // this test uses xrenderBlendPicture - the only idea is to trigger the creation
    // closing the application should not crash
    // see BUG 363251
    const auto picture = como::xRenderBlendPicture(0.5);
    // and a second one
    const auto picture2 = como::xRenderBlendPicture(0.6);
    Q_UNUSED(picture)
    Q_UNUSED(picture2)
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(BlendPictureTest)
#include "blendpicture_test.moc"
