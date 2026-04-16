/*
 * contactupdatesmanager.cpp
 * Copyright (C) 2008  Yandex LLC (Michail Pishchagin)
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

#include "contactupdatesmanager.h"

#include <QTimer>

#include "psiaccount.h"
#include "psicontact.h"
#include "psicon.h"
#include "psievent.h"
#include "userlist.h"

#ifdef YAPSI_ACTIVEX_SERVER
#include "yaonline.h"
#endif

//----------------------------------------------------------------------------
// ContactUpdatesNotifier
//----------------------------------------------------------------------------

ContactUpdatesNotifier::ContactUpdatesNotifier(ContactUpdatesManager* parent)
	: QObject(parent)
{
	connect(parent, SIGNAL(contactAction(ContactUpdatesNotifier::ContactUpdateActionType, PsiAccount*, const XMPP::Jid&)), SLOT(contactAction(ContactUpdatesNotifier::ContactUpdateActionType, PsiAccount*, const XMPP::Jid&)));

	updateTimer_ = new QTimer(this);
	updateTimer_->setSingleShot(false);
	updateTimer_->setInterval(10000);
	connect(updateTimer_, SIGNAL(timeout()), SLOT(update()));

	qRegisterMetaType<PsiAccount*>("PsiAccount*");
	qRegisterMetaType<XMPP::Jid>("XMPP::Jid");
}

ContactUpdatesNotifier::~ContactUpdatesNotifier()
{
}

ContactUpdatesNotifier::ContactUpdateActionType ContactUpdatesNotifier::authEventType(const QString& authType)
{
	ContactUpdatesNotifier::ContactUpdateActionType type = ContactUpdatesNotifier::ContactAuthUnsubscribed;
	if (authType == "subscribe")
		type = ContactUpdatesNotifier::ContactAuthSubscribeRequest;
	else if (authType == "subscribed")
		type = ContactUpdatesNotifier::ContactAuthSubscribed;
	else if (authType == "unsubscribed" || authType == "unsubscribe")
		type = ContactUpdatesNotifier::ContactAuthUnsubscribed;
	else
		Q_ASSERT(false);

	return type;
}

void ContactUpdatesNotifier::waitForAction(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, const XMPP::Jid& jid, bool compareResource, QObject* object, QString slot, int timeout)
{
	PendingNotification n(type, account, jid, compareResource,
	                      object, slot,
	                      timeout);
	notifications_ << n;
	updateTimer_->start();
}

bool ContactUpdatesNotifier::filterContactAuth(PsiAccount* account, const XMPP::Jid& jid, const QString authType)
{
	ContactUpdatesNotifier::ContactUpdateActionType type = authEventType(authType);

	bool filter = false;

	QMutableListIterator<PendingNotification> it(notifications_);
	while (it.hasNext()) {
		PendingNotification n = it.next();

		if (n.type == type &&
		    n.account == account &&
		    n.jid.compare(jid, n.compareResource))
		{
			QMetaObject::invokeMethod(n.object, n.slot.toLatin1().constData(), Qt::DirectConnection,
			                          Q_RETURN_ARG(bool, filter),
			                          Q_ARG(ContactUpdatesNotifier::ContactUpdateActionType, n.type),
			                          Q_ARG(PsiAccount*, n.account),
			                          Q_ARG(XMPP::Jid, n.jid),
			                          Q_ARG(bool, true));
			it.remove();
			break;
		}
	}

	return filter;
}

void ContactUpdatesNotifier::contactAction(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, const XMPP::Jid& jid)
{
	QMutableListIterator<PendingNotification> it(notifications_);
	while (it.hasNext()) {
		PendingNotification n = it.next();

		if (n.type == type &&
		    n.account == account &&
		    n.jid.compare(jid, n.compareResource))
		{
			QMetaObject::invokeMethod(n.object, n.slot.toLatin1().constData(), Qt::DirectConnection, QGenericReturnArgument(),
			                          Q_ARG(ContactUpdatesNotifier::ContactUpdateActionType, n.type),
			                          Q_ARG(PsiAccount*, n.account),
			                          Q_ARG(XMPP::Jid, n.jid),
			                          Q_ARG(bool, true));
			it.remove();
		}
	}
}

void ContactUpdatesNotifier::update()
{
	QDateTime now = QDateTime::currentDateTime();
	QMutableListIterator<PendingNotification> it(notifications_);
	while (it.hasNext()) {
		PendingNotification n = it.next();

		if (qAbs(now.secsTo(n.startTime)) > n.timeout) {
			QMetaObject::invokeMethod(n.object, n.slot.toLatin1().constData(), Qt::DirectConnection, QGenericReturnArgument(),
			                          Q_ARG(ContactUpdatesNotifier::ContactUpdateActionType, n.type),
			                          Q_ARG(PsiAccount*, n.account),
			                          Q_ARG(XMPP::Jid, n.jid),
			                          Q_ARG(bool, false));
			it.remove();
		}
	}

	if (notifications_.empty())
		updateTimer_->stop();
	else
		updateTimer_->start();
}

bool ContactUpdatesNotifier::isRegisteredJid(const QString& jid) const
{
	return registeredJids_.contains(jid);
}

QString ContactUpdatesNotifier::desiredName(const QString& jid) const
{
	if (!registeredJids_.contains(jid))
		return QString();
	return registeredJids_[jid];
}

void ContactUpdatesNotifier::registerJid(const QString& jid, const QString& desiredName)
{
	registeredJids_[jid] = desiredName;
}

//----------------------------------------------------------------------------
// ContactUpdatesManager
//----------------------------------------------------------------------------

ContactUpdatesManager::ContactUpdatesManager(PsiCon* parent)
	: QObject(parent)
	, controller_(parent)
{
	Q_ASSERT(controller_);
	updateTimer_ = new QTimer(this);
	updateTimer_->setSingleShot(false);
	updateTimer_->setInterval(0);
	connect(updateTimer_, SIGNAL(timeout()), SLOT(update()));

	notifier_ = new ContactUpdatesNotifier(this);
}

ContactUpdatesManager::~ContactUpdatesManager()
{
}

ContactUpdatesNotifier* ContactUpdatesManager::notifier() const
{
	return notifier_;
}

void ContactUpdatesManager::addAction(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	updates_ << ContactUpdateAction(type, account, jid);
	updateTimer_->start();
}

void ContactUpdatesManager::contactBlocked(PsiAccount* account, const XMPP::Jid& jid)
{
	addAction(ContactUpdatesNotifier::ContactBlocked, account, jid);
}

void ContactUpdatesManager::contactDeauthorized(PsiAccount* account, const XMPP::Jid& jid)
{
	addAction(ContactUpdatesNotifier::ContactDeauthorized, account, jid);
}

void ContactUpdatesManager::contactAuthorized(PsiAccount* account, const XMPP::Jid& jid)
{
	addAction(ContactUpdatesNotifier::ContactAuthorized, account, jid);
}

void ContactUpdatesManager::contactRemoved(PsiAccount* account, const XMPP::Jid& jid)
{
	addAction(ContactUpdatesNotifier::ContactRemoved, account, jid);

	// we must act immediately, since otherwise all corresponding events
	// will be simply deleted
	removeAuthRequestEventsFor(account, jid, true);
	removeToastersFor(account, jid);
}

void ContactUpdatesManager::contactAdded(PsiAccount* account, const XMPP::Jid& jid)
{
	addAction(ContactUpdatesNotifier::ContactAdded, account, jid);
}

void ContactUpdatesManager::contactRemoveFinished(PsiAccount* account, const XMPP::Jid& jid)
{
	addAction(ContactUpdatesNotifier::ContactRemoveFinished, account, jid);
}

// void ContactUpdatesManager::contactAuth(PsiAccount* account, const XMPP::Jid& jid, const QString authType)
// {
// 	ContactUpdatesNotifier::ContactUpdateActionType type = ContactUpdatesNotifier::authEventType(authType);
// 	addAction(type, account, jid);
// }

void ContactUpdatesManager::groupchatActivated(PsiAccount* account, const XMPP::Jid& jid)
{
	addAction(ContactUpdatesNotifier::GroupchatActivated, account, jid);
}

void ContactUpdatesManager::removeAuthRequestEventsFor(PsiAccount* account, const XMPP::Jid& jid, bool denyAuthRequests)
{
	Q_ASSERT(account);
	if (!account || !controller_)
		return;

	foreach(EventQueue::PsiEventId p, account->eventQueue()->eventsFor(jid, false)) {
		PsiEvent* e = p.second;
		if (e->type() == PsiEvent::Auth) {
			AuthEvent* authEvent = static_cast<AuthEvent*>(e);
			if (authEvent->authType() == "subscribe") {
				if (denyAuthRequests) {
					account->dj_deny(jid);
				}
#ifdef YAPSI_ACTIVEX_SERVER
				controller_->yaOnline()->closeNotify(p.first, e);
#endif
				account->eventQueue()->dequeue(e);
				e->deleteLater();
			}
		}
	}
}

void ContactUpdatesManager::removeToastersFor(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	if (!account || !controller_)
		return;

	foreach(EventQueue::PsiEventId p, account->eventQueue()->eventsFor(jid, false)) {
		PsiEvent* e = p.second;
		if (e->type() == PsiEvent::Mood ||
		    e->type() == PsiEvent::Message ||
		    e->type() == PsiEvent::GroupchatAlert ||
		    e->type() == PsiEvent::GroupchatInvite)
		{
#ifdef YAPSI_ACTIVEX_SERVER
			controller_->yaOnline()->closeNotify(p.first, e);
#endif
			account->eventQueue()->dequeue(e);
			e->deleteLater();
		}
	}
}

void ContactUpdatesManager::removeNotInListContacts(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	if (!account)
		return;

	foreach(UserListItem* u, account->findRelevant(jid)) {
		if (u && !u->inList()) {
			account->actionRemove(u->jid());
		}
	}
}

void ContactUpdatesManager::update()
{
	while (!updates_.isEmpty()) {
		ContactUpdateAction action = updates_.takeFirst();
		if (!action.account)
			continue;

		if (action.type == ContactUpdatesNotifier::ContactBlocked) {
			removeAuthRequestEventsFor(action.account, action.jid, true);
			removeNotInListContacts(action.account, action.jid);
		}
		else if (action.type == ContactUpdatesNotifier::ContactAuthorized) {
			removeAuthRequestEventsFor(action.account, action.jid, false);
		}
		else if (action.type == ContactUpdatesNotifier::ContactDeauthorized) {
			removeAuthRequestEventsFor(action.account, action.jid, false);
			removeNotInListContacts(action.account, action.jid);
		}
		else if (action.type == ContactUpdatesNotifier::GroupchatActivated) {
			removeToastersFor(action.account, action.jid);
		}

		emit contactAction(action.type, action.account, action.jid);
	}

	if (updates_.isEmpty())
		updateTimer_->stop();
	else
		updateTimer_->start();
}
