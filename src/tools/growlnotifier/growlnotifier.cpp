/*
 * growlnotifier.cpp - QSystemTrayIcon-based stub replacing Growl
 *
 * Copyright (C) 2005-2011  Remko Troncon, Michail Pishchagin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * You can also redistribute and/or modify this program under the
 * terms of the Psi License, specified in the accompanied COPYING
 * file, as published by the Psi Project; either dated January 1st,
 * 2005, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "growlnotifier.h"
#include <QPixmap>
#include <QString>
#include <QStringList>

class GrowlNotifier::Private {
public:
    QString appName;
};

GrowlNotifier::GrowlNotifier(QObject* parent, const QStringList& /*notifications*/,
                              const QStringList& /*defaults*/, const QString& appName)
    : QObject(parent), d(new Private)
{
    d->appName = appName;
}

GrowlNotifier::~GrowlNotifier()
{
    delete d;
}

void GrowlNotifier::notify(const QString& /*name*/, const QString& title,
                            const QString& description, const QPixmap& /*icon*/,
                            bool /*sticky*/, const QObject* /*receiver*/,
                            const char* /*clicked_slot*/, const char* /*timeout_slot*/,
                            void* /*context*/)
{
    // Growl replaced with no-op stub. Tray notifications handled by PsiTrayIcon.
    Q_UNUSED(title);
    Q_UNUSED(description);
}
