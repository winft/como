/*
  'Web' kwin client

  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

#ifndef KWIN_WEB_BUTTON_LOWER_H
#define KWIN_WEB_BUTTON_LOWER_H

#include "WebButton.h"

namespace Web
{

  class WebButtonLower : public WebButton
  {
    Q_OBJECT

    public:

      WebButtonLower(QWidget * parent, WebClient* deco);

    protected:

      virtual void clickEvent(int button);

    signals:

      void lowerWindow();
  };
}

#endif
// vim:ts=2:sw=2:tw=78:set et:
