/*
SPDX-FileCopyrightText: 2009, 2010, 2012 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef AURORAE_H
#define AURORAE_H

#include <KDecoration2/DecoratedClient>
#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationThemeProvider>
#include <KPluginMetaData>
#include <QElapsedTimer>
#include <QQuickItem>
#include <QVariant>

class QQmlComponent;
class QQmlContext;
class QQmlEngine;

class KConfigLoader;

namespace como
{
class Borders;
class OffscreenQuickView;
}

namespace Aurorae
{

class Decoration : public KDecoration2::Decoration
{
    Q_OBJECT
    Q_PROPERTY(KDecoration2::DecoratedClient* client READ client CONSTANT)
    Q_PROPERTY(QQuickItem* item READ item)
public:
    explicit Decoration(QObject* parent = nullptr, const QVariantList& args = QVariantList());
    ~Decoration() override;

    void paint(QPainter* painter, const QRect& repaintRegion) override;

    Q_INVOKABLE QVariant readConfig(const QString& key, const QVariant& defaultValue = QVariant());

    QQuickItem* item() const;

public Q_SLOTS:
    bool init() override;
    void installTitleItem(QQuickItem* item);

    void updateShadow();
    void updateBlur();

Q_SIGNALS:
    void configChanged();

protected:
    void hoverEnterEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void setupBorders(QQuickItem* item);
    void updateBorders();
    void updateBuffer();
    void updateExtendedBorders();

    bool m_supportsMask{false};

    QRect m_contentRect; // the geometry of the part of the buffer that is not a shadow when buffer
                         // was created.
    QQuickItem* m_item = nullptr;
    QQmlContext* m_qmlContext = nullptr;
    como::Borders* m_borders;
    como::Borders* m_maximizedBorders;
    como::Borders* m_extendedBorders;
    como::Borders* m_padding;
    QString m_themeName;

    std::unique_ptr<como::OffscreenQuickView> m_view;
};

class ThemeProvider : public KDecoration2::DecorationThemeProvider
{
    Q_OBJECT
public:
    explicit ThemeProvider(QObject* parent, const KPluginMetaData& data);

    QList<KDecoration2::DecorationThemeMetaData> themes() const override
    {
        return m_themes;
    }

private:
    void init();
    void findAllQmlThemes();
    void findAllSvgThemes();
    bool hasConfiguration(const QString& theme);
    QList<KDecoration2::DecorationThemeMetaData> m_themes;
    const KPluginMetaData m_data;
};

}

#endif
