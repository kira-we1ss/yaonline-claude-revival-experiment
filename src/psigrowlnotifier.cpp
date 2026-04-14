/*
 * psigrowlnotifier.cpp: Psi's interface to Growl
 * Copyright (C) 2005  Remko Troncon
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

#include <QPixmap>
#include <QStringList>
#include <QCoreApplication>

#include "common.h"
#include "psiaccount.h"
#include "avatars.h"
#include "growlnotifier.h"
#include "psigrowlnotifier.h"
#include "psievent.h"
#include "userlist.h"

#ifdef YAPSI
#include "yacommon.h"
#include "psioptions.h"
#include "psicontact.h"
#include "yavisualutil.h"
#endif

/**
 * A class representing the notification context, which will be passed to
 * Growl, and then passed back when a notification is clicked.
 */
class NotificationContext
{
public:
	NotificationContext(PsiAccount* a, Jid j) : account_(a), jid_(j) { }
	PsiAccount* account() { return account_; }
	Jid jid() { return jid_; }


private:
	PsiAccount* account_;
	Jid jid_;
};


/**
 * (Private) constructor of the PsiGrowlNotifier.
 * Initializes notifications and registers with Growl through GrowlNotifier.
 */
PsiGrowlNotifier::PsiGrowlNotifier(QObject* parent, const QStringList& notifications, const QStringList& defaults, const QString& appName)
	: PsiNotifierBase(parent, appName)
{
	gn_ = new GrowlNotifier(this, notifications, defaults, appName);
}

void PsiGrowlNotifier::notify(PsiAccount* account, const XMPP::Jid& jid, const QString& name, const QString& title, const QString& desc, const QPixmap& icon)
{
	NotificationContext* context = 0;
	if (!jid.isNull()) {
		context = new NotificationContext(account, jid);
	}

	gn_->notify(name, title, desc, icon,
	            false, this,
	            context ? "notificationClicked" : 0,
	            context ? "notificationTimedOut" : 0,
	            context);
}

void PsiGrowlNotifier::notificationClicked(void* c)
{
	NotificationContext* context = (NotificationContext*) c;
	context->account()->actionDefault(context->jid());
	delete context;
}

void PsiGrowlNotifier::notificationTimedOut(void* c)
{
	NotificationContext* context = (NotificationContext*) c;
	delete context;
}
