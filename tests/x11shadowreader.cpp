/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only
*/
#include "base/x11/xcb/atom.h"
#include "base/x11/xcb/property.h"
#include "base/x11/xcb/proto.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGui/private/qtx11extras_p.h>

static QVector<uint32_t> readShadow(quint32 windowId)
{
    auto con = QX11Info::connection();
    como::base::x11::xcb::atom atom(QByteArrayLiteral("_KDE_NET_WM_SHADOW"), false, con);
    QVector<uint32_t> ret;

    if (windowId != XCB_WINDOW) {
        como::base::x11::xcb::property property(
            con, false, windowId, atom, XCB_ATOM_CARDINAL, 0, 12);
        uint32_t* shadow = property.value<uint32_t*>();
        if (shadow) {
            ret.reserve(12);
            for (int i = 0; i < 12; ++i) {
                ret << shadow[i];
            }
        } else {
            qDebug() << "!!!! no shadow";
        }
    } else {
        qDebug() << "!!!! no window";
    }
    return ret;
}

static QVector<QPixmap> getPixmaps(const QVector<uint32_t>& data)
{
    QVector<QPixmap> ret;
    static const int ShadowElementsCount = 8;
    QVector<como::base::x11::xcb::geometry> pixmapGeometries;
    QVector<xcb_get_image_cookie_t> getImageCookies(ShadowElementsCount);
    auto c = QX11Info::connection();
    for (int i = 0; i < ShadowElementsCount; ++i) {
        pixmapGeometries.push_back(como::base::x11::xcb::geometry(c, data[i]));
    }
    auto discardReplies = [&getImageCookies](int start) {
        for (int i = start; i < getImageCookies.size(); ++i) {
            xcb_discard_reply(QX11Info::connection(), getImageCookies.at(i).sequence);
        }
    };
    for (int i = 0; i < ShadowElementsCount; ++i) {
        auto& geo = pixmapGeometries[i];
        if (geo.is_null()) {
            discardReplies(0);
            return QVector<QPixmap>();
        }
        getImageCookies[i] = xcb_get_image_unchecked(
            c, XCB_IMAGE_FORMAT_Z_PIXMAP, data[i], 0, 0, geo->width, geo->height, ~0);
    }
    for (int i = 0; i < ShadowElementsCount; ++i) {
        auto* reply = xcb_get_image_reply(c, getImageCookies.at(i), nullptr);
        if (!reply) {
            discardReplies(i + 1);
            return QVector<QPixmap>();
        }
        auto& geo = pixmapGeometries[i];
        QImage image(xcb_get_image_data(reply), geo->width, geo->height, QImage::Format_ARGB32);
        ret << QPixmap::fromImage(image);
        free(reply);
    }
    return ret;
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "xcb");
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument(QStringLiteral("windowId"),
                                 QStringLiteral("The X11 windowId from which to read the shadow"));
    parser.addHelpOption();
    parser.process(app);

    if (parser.positionalArguments().count() != 1) {
        parser.showHelp(1);
    }

    bool ok = false;
    const auto shadow = readShadow(parser.positionalArguments().constFirst().toULongLong(&ok, 16));
    if (!ok) {
        qDebug() << "!!! Failed to read window id";
        return 1;
    }
    if (shadow.isEmpty()) {
        qDebug() << "!!!! Read shadow failed";
        return 1;
    }
    const auto pixmaps = getPixmaps(shadow);
    if (pixmaps.isEmpty()) {
        qDebug() << "!!!! Read pixmap failed";
        return 1;
    }

    QScopedPointer<QWidget> widget(new QWidget());
    QFormLayout* layout = new QFormLayout(widget.data());
    for (const auto& pix : pixmaps) {
        QLabel* l = new QLabel(widget.data());
        l->setPixmap(pix);
        layout->addRow(QStringLiteral("%1x%2:").arg(pix.width()).arg(pix.height()), l);
    }
    widget->setLayout(layout);
    widget->show();
    return app.exec();
}
