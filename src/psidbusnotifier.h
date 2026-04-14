/*
 * psidbusnotifier.h
 * Copyright (C) 2010  Yandex LLC (Michail Pishchagin)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#ifndef PSIDBUSNOTIFIER_H
#define PSIDBUSNOTIFIER_H

#include "psinotifierbase.h"

#include <QPointer>
#include <QMap>

class PsiDbusNotifier : public PsiNotifierBase
{
	Q_OBJECT
public:
	PsiDbusNotifier(QObject* parent, QString appName);
	~PsiDbusNotifier();

	// reimplemented
	virtual void notify(PsiAccount* account, const XMPP::Jid& jid, const QString& name, const QString& title, const QString& desc, const QPixmap& icon);

private:
	struct NotificationData {
		QPointer<PsiAccount> account;
		XMPP::Jid jid;
	};
	QMap<int, NotificationData> notifications_;
};

#endif
