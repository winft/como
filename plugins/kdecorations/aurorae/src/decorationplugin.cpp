/*
SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

SPDX-License-Identifier: GPL-2.0-or-later

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "decorationplugin.h"
#include "colorhelper.h"
#include "decorationoptions.h"
#include <QtQml>

void DecorationPlugin::registerTypes(const char* uri)
{
    Q_ASSERT(QLatin1String(uri) == QLatin1String("org.kde.kwin.decoration"));
    qmlRegisterType<ColorHelper>(uri, 0, 1, "ColorHelper");
    qmlRegisterType<como::DecorationOptions>(uri, 0, 1, "DecorationOptions");
    qmlRegisterType<como::Borders>(uri, 0, 1, "Borders");
}
