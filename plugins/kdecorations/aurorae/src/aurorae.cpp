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
#include "aurorae.h"

#include "auroraeshared.h"
#include "auroraetheme.h"
#include "decorationoptions.h"

#include <como/base/config-como.h>
#include <como/render/effect/interface/effects_handler.h>
#include <como/render/effect/interface/offscreen_quick_view.h>

// KDecoration2
#include <KDecoration2/DecoratedClient>
#include <KDecoration2/DecorationSettings>
#include <KDecoration2/DecorationShadow>
// KDE
#include <KConfigGroup>
#include <KConfigLoader>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KLocalizedTranslator>
#include <KPackage/PackageLoader>
#include <KPluginFactory>
#include <KSharedConfig>
// Qt
#include <QDebug>
#include <QDirIterator>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

K_PLUGIN_FACTORY_WITH_JSON(AuroraeDecoFactory,
                           "aurorae.json",
                           registerPlugin<Aurorae::Decoration>();
                           registerPlugin<Aurorae::ThemeProvider>();)

namespace Aurorae
{

class Helper
{
public:
    void ref();
    void unref();
    QQmlComponent* component(const QString& theme);
    QQmlContext* rootContext();
    QQmlComponent* svgComponent()
    {
        return m_svgComponent.data();
    }

    static Helper& instance();

private:
    Helper() = default;
    void init();
    QQmlComponent* loadComponent(const QString& themeName);
    int m_refCount = 0;
    QScopedPointer<QQmlEngine> m_engine;
    QHash<QString, QQmlComponent*> m_components;
    QScopedPointer<QQmlComponent> m_svgComponent;
};

Helper& Helper::instance()
{
    static Helper s_helper;
    return s_helper;
}

void Helper::ref()
{
    m_refCount++;
    if (m_refCount == 1) {
        m_engine.reset(new QQmlEngine);
        init();
    }
}

void Helper::unref()
{
    m_refCount--;
    if (m_refCount == 0) {
        // cleanup
        m_svgComponent.reset();
        m_engine.reset();
        m_components.clear();
    }
}

static const QString s_defaultTheme = QStringLiteral("kwin4_decoration_qml_plastik");
static const QString s_qmlPackageFolder = QStringLiteral("kwin/decorations/");

QQmlComponent* Helper::component(const QString& themeName)
{
    // maybe it's an SVG theme?
    if (themeName.startsWith(QLatin1String("__aurorae__svg__"))) {
        if (m_svgComponent.isNull()) {
            /* use logic from KDeclarative::setupBindings():
            "addImportPath adds the path at the beginning, so to honour user's
            paths we need to traverse the list in reverse order" */
            QStringListIterator paths(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                                                QStringLiteral("module/imports"),
                                                                QStandardPaths::LocateDirectory));
            paths.toBack();
            while (paths.hasPrevious()) {
                m_engine->addImportPath(paths.previous());
            }
            m_svgComponent.reset(new QQmlComponent(m_engine.data()));
            m_svgComponent->loadUrl(QUrl(QStandardPaths::locate(
                QStandardPaths::GenericDataLocation, QStringLiteral("kwin/aurorae/aurorae.qml"))));
        }
        // verify that the theme exists
        if (!QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                    QStringLiteral("aurorae/themes/%1/%1rc").arg(themeName.mid(16)))
                 .isEmpty()) {
            return m_svgComponent.data();
        }
    }
    // try finding the QML package
    auto it = m_components.constFind(themeName);
    if (it != m_components.constEnd()) {
        return it.value();
    }
    auto component = loadComponent(themeName);
    if (component) {
        m_components.insert(themeName, component);
        return component;
    }
    // try loading default component
    if (themeName != s_defaultTheme) {
        return loadComponent(s_defaultTheme);
    }
    return nullptr;
}

QQmlComponent* Helper::loadComponent(const QString& themeName)
{
    qCDebug(AURORAE) << "Trying to load QML Decoration " << themeName;
    const QString internalname = themeName.toLower();

    const auto offers = KPackage::PackageLoader::self()->findPackages(
        QStringLiteral("KWin/Decoration"),
        s_qmlPackageFolder,
        [internalname](const KPluginMetaData& data) {
            return data.pluginId().compare(internalname, Qt::CaseInsensitive) == 0;
        });
    if (offers.isEmpty()) {
        qCCritical(AURORAE) << "Couldn't find QML Decoration " << themeName;
        // TODO: what to do in error case?
        return nullptr;
    }
    const KPluginMetaData service = offers.first();
    const QString pluginName = service.pluginId();
    const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                                s_qmlPackageFolder + pluginName
                                                    + QLatin1String("/contents/ui/main.qml"));
    if (file.isNull()) {
        qCDebug(AURORAE) << "Could not find script file for " << pluginName;
        // TODO: what to do in error case?
        return nullptr;
    }
    // setup the QML engine
    /* use logic from KDeclarative::setupBindings():
    "addImportPath adds the path at the beginning, so to honour user's
     paths we need to traverse the list in reverse order" */
    QStringListIterator paths(QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                                        QStringLiteral("module/imports"),
                                                        QStandardPaths::LocateDirectory));
    paths.toBack();
    while (paths.hasPrevious()) {
        m_engine->addImportPath(paths.previous());
    }
    QQmlComponent* component = new QQmlComponent(m_engine.data(), m_engine.data());
    component->loadUrl(QUrl::fromLocalFile(file));
    return component;
}

QQmlContext* Helper::rootContext()
{
    return m_engine->rootContext();
}

void Helper::init()
{
    // we need to first load our decoration plugin
    // once it's loaded we can provide the Borders and access them from C++ side
    // so let's try to locate our plugin:
    QString pluginPath;
    auto const path_list = m_engine->importPathList();
    for (const QString& path : path_list) {
        QDirIterator it(path, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QFileInfo fileInfo = it.fileInfo();
            if (!fileInfo.isFile()) {
                continue;
            }
            if (!fileInfo.path().endsWith(QLatin1String("/org/kde/kwin/decoration"))) {
                continue;
            }
            if (fileInfo.fileName() == QLatin1String("libdecorationplugin.so")) {
                pluginPath = fileInfo.absoluteFilePath();
                break;
            }
        }
        if (!pluginPath.isEmpty()) {
            break;
        }
    }
    m_engine->importPlugin(pluginPath, "org.kde.kwin.decoration", nullptr);
    qmlRegisterType<como::Borders>("org.kde.kwin.decoration", 0, 1, "Borders");

    qmlRegisterAnonymousType<KDecoration2::Decoration>("org.kde.kwin.decoration", 0);
    qmlRegisterAnonymousType<KDecoration2::DecoratedClient>("org.kde.kwin.decoration", 0);
    qRegisterMetaType<KDecoration2::BorderSize>();
}

Decoration::Decoration(QObject* parent, const QVariantList& args)
    : KDecoration2::Decoration(parent, args)
    , m_item(nullptr)
    , m_borders(nullptr)
    , m_maximizedBorders(nullptr)
    , m_extendedBorders(nullptr)
    , m_padding(nullptr)
    , m_themeName(s_defaultTheme)
    , m_view(nullptr)
{
    m_themeName = findTheme(args);
    Helper::instance().ref();
}

Decoration::~Decoration()
{
    delete m_qmlContext;
    m_view.reset();
    Helper::instance().unref();
}

bool Decoration::init()
{
    Helper::instance().rootContext()->setContextProperty(QStringLiteral("decorationSettings"),
                                                         settings().get());
    auto s = settings();
    connect(
        s.get(), &KDecoration2::DecorationSettings::reconfigured, this, &Decoration::configChanged);

    m_qmlContext = new QQmlContext(Helper::instance().rootContext(), this);
    m_qmlContext->setContextProperty(QStringLiteral("decoration"), this);
    auto component = Helper::instance().component(m_themeName);
    if (!component) {
        return false;
    }
    if (component == Helper::instance().svgComponent()) {
        // load SVG theme
        const QString themeName = m_themeName.mid(16);
        KConfig config(QLatin1String("aurorae/themes/") + themeName + QLatin1Char('/') + themeName
                           + QLatin1String("rc"),
                       KConfig::FullConfig,
                       QStandardPaths::GenericDataLocation);
        AuroraeTheme* theme = new AuroraeTheme(this);
        theme->loadTheme(themeName, config);
        theme->setBorderSize(s->borderSize());
        connect(s.get(),
                &KDecoration2::DecorationSettings::borderSizeChanged,
                theme,
                &AuroraeTheme::setBorderSize);
        auto readButtonSize = [this, theme] {
            const KSharedConfigPtr conf = KSharedConfig::openConfig(QStringLiteral("auroraerc"));
            const KConfigGroup themeGroup(conf, m_themeName.mid(16));
            theme->setButtonSize(static_cast<KDecoration2::BorderSize>(
                (themeGroup.readEntry<int>("ButtonSize",
                                           int(KDecoration2::BorderSize::Normal) - s_indexMapper)
                 + s_indexMapper)));
        };
        connect(this, &Decoration::configChanged, theme, readButtonSize);
        readButtonSize();
        //         m_theme->setTabDragMimeType(tabDragMimeType());
        m_qmlContext->setContextProperty(QStringLiteral("auroraeTheme"), theme);
    }
    m_item = qobject_cast<QQuickItem*>(component->create(m_qmlContext));
    if (!m_item) {
        if (component->isError()) {
            const auto errors = component->errors();
            for (const auto& error : errors) {
                qCWarning(AURORAE) << error;
            }
        }
        return false;
    }

    m_item->setParent(m_qmlContext);

    QVariant visualParent = property("visualParent");
    if (visualParent.isValid()) {
        m_item->setParentItem(visualParent.value<QQuickItem*>());
        visualParent.value<QQuickItem*>()->setProperty("drawBackground", false);
    } else {
        m_view = std::make_unique<como::OffscreenQuickView>(
            como::OffscreenQuickView::ExportMode::Image);
        m_item->setParentItem(m_view->contentItem());
        auto updateSize = [this]() { m_item->setSize(m_view->contentItem()->size()); };
        updateSize();
        connect(m_view->contentItem(), &QQuickItem::widthChanged, m_item, updateSize);
        connect(m_view->contentItem(), &QQuickItem::heightChanged, m_item, updateSize);
        connect(m_view.get(),
                &como::OffscreenQuickView::repaintNeeded,
                this,
                &Decoration::updateBuffer);
    }

    m_supportsMask = m_item->property("supportsMask").toBool();

    setupBorders(m_item);

    // TODO: Is there a more efficient way to react to border changes?
    auto trackBorders = [this](como::Borders* borders) {
        if (!borders) {
            return;
        }
        connect(borders, &como::Borders::leftChanged, this, &Decoration::updateBorders);
        connect(borders, &como::Borders::rightChanged, this, &Decoration::updateBorders);
        connect(borders, &como::Borders::topChanged, this, &Decoration::updateBorders);
        connect(borders, &como::Borders::bottomChanged, this, &Decoration::updateBorders);
    };
    trackBorders(m_borders);
    trackBorders(m_maximizedBorders);
    if (m_extendedBorders) {
        updateExtendedBorders();
        connect(m_extendedBorders,
                &como::Borders::leftChanged,
                this,
                &Decoration::updateExtendedBorders);
        connect(m_extendedBorders,
                &como::Borders::rightChanged,
                this,
                &Decoration::updateExtendedBorders);
        connect(m_extendedBorders,
                &como::Borders::topChanged,
                this,
                &Decoration::updateExtendedBorders);
        connect(m_extendedBorders,
                &como::Borders::bottomChanged,
                this,
                &Decoration::updateExtendedBorders);
    }

    auto decorationClient = client();
    connect(decorationClient,
            &KDecoration2::DecoratedClient::maximizedChanged,
            this,
            &Decoration::updateBorders);
    connect(decorationClient,
            &KDecoration2::DecoratedClient::shadedChanged,
            this,
            &Decoration::updateBorders);
    updateBorders();
    if (m_view) {
        auto resizeWindow = [this] {
            QRect rect(QPoint(0, 0), size());
            if (m_padding && !client()->isMaximized()) {
                rect = rect.adjusted(
                    -m_padding->left(), -m_padding->top(), m_padding->right(), m_padding->bottom());
            }
            m_view->setGeometry(rect);
            updateBlur();
        };
        connect(this, &Decoration::bordersChanged, this, resizeWindow);
        connect(client(), &KDecoration2::DecoratedClient::widthChanged, this, resizeWindow);
        connect(client(), &KDecoration2::DecoratedClient::heightChanged, this, resizeWindow);
        connect(client(), &KDecoration2::DecoratedClient::maximizedChanged, this, resizeWindow);
        connect(client(), &KDecoration2::DecoratedClient::shadedChanged, this, resizeWindow);
        resizeWindow();
        updateBuffer();
    } else {
        // create a dummy shadow for the configuration interface
        if (m_padding) {
            auto s = std::make_shared<KDecoration2::DecorationShadow>();
            s->setPadding(*m_padding);
            s->setInnerShadowRect(QRect(m_padding->left(), m_padding->top(), 1, 1));
            setShadow(s);
        }
    }
    return true;
}

QVariant Decoration::readConfig(const QString& key, const QVariant& defaultValue)
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStringLiteral("auroraerc"));
    return config->group(m_themeName).readEntry(key, defaultValue);
}

void Decoration::setupBorders(QQuickItem* item)
{
    m_borders = item->findChild<como::Borders*>(QStringLiteral("borders"));
    m_maximizedBorders = item->findChild<como::Borders*>(QStringLiteral("maximizedBorders"));
    m_extendedBorders = item->findChild<como::Borders*>(QStringLiteral("extendedBorders"));
    m_padding = item->findChild<como::Borders*>(QStringLiteral("padding"));
}

void Decoration::updateBorders()
{
    como::Borders* b = m_borders;
    if (client()->isMaximized() && m_maximizedBorders) {
        b = m_maximizedBorders;
    }
    if (!b) {
        return;
    }
    setBorders(*b);

    updateExtendedBorders();
}

void Decoration::paint(QPainter* painter, const QRect& repaintRegion)
{
    Q_UNUSED(repaintRegion)
    if (!m_view) {
        return;
    }

    const QImage image = m_view->bufferAsImage();
    const qreal dpr = image.devicePixelRatioF();

    QRect nativeContentRect = QRect(m_contentRect.topLeft() * dpr, m_contentRect.size() * dpr);

    painter->fillRect(rect(), Qt::transparent);
    painter->drawImage(rect(), image, nativeContentRect);
}

void Decoration::updateShadow()
{
    if (!m_view) {
        return;
    }
    bool updateShadow = false;
    const auto oldShadow = shadow();
    if (m_padding
        && (m_padding->left() > 0 || m_padding->top() > 0 || m_padding->right() > 0
            || m_padding->bottom() > 0)
        && !client()->isMaximized()) {
        if (!oldShadow) {
            updateShadow = true;
        } else {
            // compare padding
            if (oldShadow->padding() != *m_padding) {
                updateShadow = true;
            }
        }
        const QImage m_buffer = m_view->bufferAsImage();
        const qreal dpr = m_buffer.devicePixelRatioF();

        QImage img(m_buffer.size() / m_buffer.devicePixelRatioF(),
                   QImage::Format_ARGB32_Premultiplied);
        QSizeF imageSize = m_buffer.size() / m_buffer.devicePixelRatioF();
        img.fill(Qt::transparent);
        QPainter p(&img);
        // top
        p.drawImage(0, 0, m_buffer, 0, 0, imageSize.width() * dpr, m_padding->top() * dpr);
        // left
        p.drawImage(0,
                    m_padding->top(),
                    m_buffer,
                    0,
                    m_padding->top() * dpr,
                    m_padding->left() * dpr,
                    (imageSize.height() - m_padding->top()) * dpr);
        // bottom
        p.drawImage(m_padding->left(),
                    imageSize.height() - m_padding->bottom(),
                    m_buffer,
                    m_padding->left() * dpr,
                    (imageSize.height() - m_padding->bottom()) * dpr,
                    (imageSize.width() - m_padding->left()) * dpr,
                    m_padding->bottom() * dpr);
        // right
        p.drawImage(imageSize.width() - m_padding->right(),
                    m_padding->top(),
                    m_buffer,
                    (imageSize.width() - m_padding->right()) * dpr,
                    m_padding->top() * dpr,
                    m_padding->right() * dpr,
                    (imageSize.height() - m_padding->top() - m_padding->bottom()) * dpr);
        if (!updateShadow) {
            updateShadow = (oldShadow->shadow() != img);
        }
        if (updateShadow) {
            auto s = std::make_shared<KDecoration2::DecorationShadow>();
            s->setShadow(img);
            s->setPadding(*m_padding);
            s->setInnerShadowRect(
                QRect(m_padding->left(),
                      m_padding->top(),
                      imageSize.width() - m_padding->left() - m_padding->right(),
                      imageSize.height() - m_padding->top() - m_padding->bottom()));
            setShadow(s);
        }
    } else {
        if (oldShadow) {
            setShadow(nullptr);
        }
    }
}

void Decoration::hoverEnterEvent(QHoverEvent* event)
{
    if (m_view) {
        event->setAccepted(false);
        m_view->forwardMouseEvent(event);
    }
    KDecoration2::Decoration::hoverEnterEvent(event);
}

void Decoration::hoverLeaveEvent(QHoverEvent* event)
{
    if (m_view) {
        m_view->forwardMouseEvent(event);
    }
    KDecoration2::Decoration::hoverLeaveEvent(event);
}

void Decoration::hoverMoveEvent(QHoverEvent* event)
{
    if (m_view) {
        // turn a hover event into a mouse because we don't follow hovers as we don't think we have
        // focus
        QMouseEvent cloneEvent(
            QEvent::MouseMove, event->posF(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        event->setAccepted(false);
        m_view->forwardMouseEvent(&cloneEvent);
        event->setAccepted(cloneEvent.isAccepted());
    }
    KDecoration2::Decoration::hoverMoveEvent(event);
}

void Decoration::mouseMoveEvent(QMouseEvent* event)
{
    if (m_view) {
        m_view->forwardMouseEvent(event);
    }
    KDecoration2::Decoration::mouseMoveEvent(event);
}

void Decoration::mousePressEvent(QMouseEvent* event)
{
    if (m_view) {
        m_view->forwardMouseEvent(event);
    }
    KDecoration2::Decoration::mousePressEvent(event);
}

void Decoration::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_view) {
        m_view->forwardMouseEvent(event);
    }
    KDecoration2::Decoration::mouseReleaseEvent(event);
}

void Decoration::installTitleItem(QQuickItem* item)
{
    auto update = [this, item] {
        QRect rect = item->mapRectToScene(item->childrenRect()).toRect();
        if (rect.isNull()) {
            rect = item->parentItem()
                       ->mapRectToScene(QRectF(item->x(), item->y(), item->width(), item->height()))
                       .toRect();
        }
        setTitleBar(rect);
    };
    update();
    connect(item, &QQuickItem::widthChanged, this, update);
    connect(item, &QQuickItem::heightChanged, this, update);
    connect(item, &QQuickItem::xChanged, this, update);
    connect(item, &QQuickItem::yChanged, this, update);
}

void Decoration::updateExtendedBorders()
{
    // extended sizes
    const int extSize = settings()->largeSpacing();
    int extLeft = m_extendedBorders->left();
    int extRight = m_extendedBorders->right();
    int extBottom = m_extendedBorders->bottom();

    if (settings()->borderSize() == KDecoration2::BorderSize::None) {
        if (!client()->isMaximizedHorizontally()) {
            extLeft = std::max(m_extendedBorders->left(), extSize);
            extRight = std::max(m_extendedBorders->right(), extSize);
        }
        if (!client()->isMaximizedVertically()) {
            extBottom = std::max(m_extendedBorders->bottom(), extSize);
        }

    } else if (settings()->borderSize() == KDecoration2::BorderSize::NoSides
               && !client()->isMaximizedHorizontally()) {
        extLeft = std::max(m_extendedBorders->left(), extSize);
        extRight = std::max(m_extendedBorders->right(), extSize);
    }

    setResizeOnlyBorders(QMargins(extLeft, 0, extRight, extBottom));
}

void Decoration::updateBlur()
{
    if (!m_item || !m_supportsMask) {
        return;
    }

    QRegion mask;

    if (client() && client()->isMaximized()) {
        mask = QRect(0, 0, m_item->width(), m_item->height());
    } else {
        const QVariant maskProperty = m_item->property("decorationMask");
        if (static_cast<QMetaType::Type>(maskProperty.type()) == QMetaType::QRegion) {
            mask = maskProperty.value<QRegion>();

            if (!mask.isNull()) {
                // moving mask by 1,1 because mask size has already been adjusted to be smaller than
                // the frame. Since the svg will have antialiasing and the mask not, there will be
                // artifacts at the corners, if they go under the svg they're less evident.
                QPoint maskOffset(-m_padding->left() + 1, -m_padding->top() + 1);
                mask.translate(maskOffset);
            }
        }
    }

    setBlurRegion(mask);
}

void Decoration::updateBuffer()
{
    const QImage buffer = m_view->bufferAsImage();
    if (buffer.isNull()) {
        return;
    }
    m_contentRect = QRect(QPoint(0, 0), m_view->contentItem()->size().toSize());
    if (m_padding
        && (m_padding->left() > 0 || m_padding->top() > 0 || m_padding->right() > 0
            || m_padding->bottom() > 0)
        && !client()->isMaximized()) {
        m_contentRect = m_contentRect.adjusted(
            m_padding->left(), m_padding->top(), -m_padding->right(), -m_padding->bottom());
    }
    updateShadow();
    updateBlur();
    update();
}

QQuickItem* Decoration::item() const
{
    return m_item;
}

ThemeProvider::ThemeProvider(QObject* parent, const KPluginMetaData& data)
    : KDecoration2::DecorationThemeProvider(parent)
    , m_data(data)
{
    init();
}

void ThemeProvider::init()
{
    findAllQmlThemes();
    findAllSvgThemes();
}

void ThemeProvider::findAllQmlThemes()
{
    const auto offers = KPackage::PackageLoader::self()->findPackages(
        QStringLiteral("KWin/Decoration"), s_qmlPackageFolder);
    for (const auto& offer : offers) {
        KDecoration2::DecorationThemeMetaData data;
        data.setPluginId(m_data.pluginId());
        data.setThemeName(offer.pluginId());
        data.setVisibleName(offer.name());
        if (hasConfiguration(offer.pluginId())) {
            data.setConfigurationName("kcm_auroraedecoration");
        }
        m_themes.append(data);
    }
}

void ThemeProvider::findAllSvgThemes()
{
    QStringList themes;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                                       QStringLiteral("aurorae/themes/"),
                                                       QStandardPaths::LocateDirectory);
    QStringList themeDirectories;
    for (const QString& dir : dirs) {
        auto const entry_list = QDir(dir).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for (const QString& themeDir : entry_list) {
            themeDirectories << dir + themeDir;
        }
    }
    for (const QString& dir : qAsConst(themeDirectories)) {
        auto const entry_list
            = QDir(dir).entryList(QStringList() << QStringLiteral("metadata.desktop"));
        for (const QString& file : entry_list) {
            themes.append(dir + '/' + file);
        }
    }
    for (const QString& theme : themes) {
        int themeSepIndex = theme.lastIndexOf('/', -1);
        QString themeRoot = theme.left(themeSepIndex);
        int themeNameSepIndex = themeRoot.lastIndexOf('/', -1);
        QString packageName = themeRoot.right(themeRoot.length() - themeNameSepIndex - 1);

        KDesktopFile df(theme);
        QString name = df.readName();
        if (name.isEmpty()) {
            name = packageName;
        }

        KDecoration2::DecorationThemeMetaData data;
        data.setPluginId(m_data.pluginId());
        data.setThemeName(QLatin1String("__aurorae__svg__") + packageName);
        data.setVisibleName(name);
        if (hasConfiguration(data.themeName())) {
            data.setConfigurationName("kcm_auroraedecoration");
        }
        m_themes.append(data);
    }
}

bool ThemeProvider::hasConfiguration(const QString& theme)
{
    if (theme.startsWith(QLatin1String("__aurorae__svg__"))) {
        return true;
    }
    const QString ui = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("kwin/decorations/%1/contents/ui/config.ui").arg(theme));
    const QString xml = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("kwin/decorations/%1/contents/config/main.xml").arg(theme));
    return !(ui.isEmpty() || xml.isEmpty());
}

}

#include "aurorae.moc"
