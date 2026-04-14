/*
 * contactupdatesmanager.h
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

#ifndef CONTACTUPDATESMANAGER_H
#define CONTACTUPDATESMANAGER_H

#include <QObject>
#include <QPointer>
#include <QDateTime>
#include <QHash>

class PsiCon;
class QTimer;

#include "psiaccount.h"
#include "xmpp_jid.h"

class ContactUpdatesManager;

class ContactUpdatesNotifier : public QObject
{
	Q_OBJECT
public:
	ContactUpdatesNotifier(ContactUpdatesManager* parent);
	~ContactUpdatesNotifier();

	enum ContactUpdateActionType {
		ContactBlocked = 0,
		ContactAuthorized,
		ContactDeauthorized,

		ContactAdded,

		ContactRemoved,
		ContactRemoveFinished,

		// notifications must return bool
		ContactAuthSubscribeRequest,
		ContactAuthSubscribed,
		ContactAuthUnsubscribed,

		GroupchatActivated
	};

	static ContactUpdateActionType authEventType(const QString& authType);

	bool filterContactAuth(PsiAccount* account, const XMPP::Jid& jid, const QString authType);

	void waitForAction(ContactUpdateActionType type, PsiAccount* account, const XMPP::Jid& jid, bool compareResource,
	                   QObject* object, QString slot,
	                   int timeout = 30);

	bool isRegisteredJid(const QString& jid) const;
	QString desiredName(const QString& jid) const;
	void registerJid(const QString& jid, const QString& desiredName);

private slots:
	void contactAction(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, const XMPP::Jid& jid);
	void update();

private:
	struct PendingNotification {
		PendingNotification(ContactUpdateActionType _type, PsiAccount* _account, const XMPP::Jid& _jid, bool _compareResource,
		                   QObject* _object, QString _slot,
		                   int _timeout)
			: startTime(QDateTime::currentDateTime())
			, type(_type)
			, account(_account)
			, jid(_jid)
			, compareResource(_compareResource)
			, object(_object)
			, slot(_slot)
			, timeout(_timeout)
		{
		}

		QDateTime startTime;

		ContactUpdateActionType type;
		QPointer<PsiAccount> account;
		XMPP::Jid jid;
		bool compareResource;

		QPointer<QObject> object;
		QString slot;

		int timeout;
	};
	QList<PendingNotification> notifications_;
	QTimer* updateTimer_;
	QHash<QString, QString> registeredJids_;
};

class ContactUpdatesManager : public QObject
{
	Q_OBJECT
public:
	ContactUpdatesManager(PsiCon* parent);
	~ContactUpdatesManager();

	ContactUpdatesNotifier* notifier() const;

	void contactBlocked(PsiAccount* account, const XMPP::Jid& jid);
	void contactDeauthorized(PsiAccount* account, const XMPP::Jid& jid);
	void contactAuthorized(PsiAccount* account, const XMPP::Jid& jid);
	void contactAdded(PsiAccount* account, const XMPP::Jid& jid);
	void contactRemoved(PsiAccount* account, const XMPP::Jid& jid);
	void contactRemoveFinished(PsiAccount* account, const XMPP::Jid& jid);
	// void contactAuth(PsiAccount* account, const XMPP::Jid& jid, const QString authType);

	void groupchatActivated(PsiAccount* account, const XMPP::Jid& jid);

signals:
	void contactAction(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, const XMPP::Jid& jid);

private slots:
	void update();

private:
	QPointer<PsiCon> controller_;
	struct ContactUpdateAction {
		ContactUpdateAction(ContactUpdatesNotifier::ContactUpdateActionType _type, PsiAccount* _account, const XMPP::Jid& _jid)
			: type(_type)
			, account(_account)
			, jid(_jid)
		{}
		ContactUpdatesNotifier::ContactUpdateActionType type;
		QPointer<PsiAccount> account;
		XMPP::Jid jid;
	};
	QList<ContactUpdateAction> updates_;
	QTimer* updateTimer_;
	ContactUpdatesNotifier* notifier_;

	void addAction(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, const XMPP::Jid& jid);
	void removeAuthRequestEventsFor(PsiAccount* account, const XMPP::Jid& jid, bool denyAuthRequests);
	void removeToastersFor(PsiAccount* account, const XMPP::Jid& jid);
	void removeNotInListContacts(PsiAccount* account, const XMPP::Jid& jid);
};

#endif