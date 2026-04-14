/*
 * deliveryconfirmationmanager.cpp
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

#include "deliveryconfirmationmanager.h"

#include <QDomElement>
#include <QTimer>
#ifdef Q_WS_WIN
#include <QSettings>
#endif

#include "xmpp_xmlcommon.h"
#include "xmpp_task.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "capsmanager.h"
#include "userlist.h"
#include "pongserver.h"
#include "psilogger.h"
#include "dummystream.h"
#include "xmpp_client.h"

static const int minimumPingInterval = 5; // in seconds

DeliveryConfirmationManager::DeliveryConfirmationManager(PsiAccount* account)
	: QObject(account)
	, account_(account)
{
	timer_ = new QTimer(this);
	timer_->setSingleShot(false);
	timer_->setInterval(10000);
	connect(timer_, SIGNAL(timeout()), SLOT(timeout()));

	requestPingTimer_ = new QTimer(this);
	requestPingTimer_->setSingleShot(true);
	requestPingTimer_->setInterval(1 * 1000);
	connect(requestPingTimer_, SIGNAL(timeout()), SLOT(requestPing()));

	idlePingTimer_ = new QTimer(this);
	idlePingTimer_->setSingleShot(false);
	idlePingTimer_->setInterval(25 * 1000);
	connect(idlePingTimer_, SIGNAL(timeout()), SLOT(idlePing()));

	pingTimeoutTimer_ = new QTimer(this);
	pingTimeoutTimer_->setSingleShot(true);
	pingTimeoutTimer_->setInterval(45 * 1000);
	connect(pingTimeoutTimer_, SIGNAL(timeout()), SLOT(pingTimeout()));

	logAllMessages_ = PsiLogger::isLogAllMessagesEnabled();

	connect(account, SIGNAL(enabledChanged()), SLOT(accountEnabledChanged()));
	accountEnabledChanged();
}

DeliveryConfirmationManager::~DeliveryConfirmationManager()
{
}

void DeliveryConfirmationManager::timeout()
{
	static const int deliveryConfirmationTimeoutSecs = 60;
	QDateTime currentDateTime = QDateTime::currentDateTime();
	QMutableHashIterator<QString, QDateTime> it(confirmations_);
	while (it.hasNext()) {
		it.next();
		if (it.value().secsTo(currentDateTime) > deliveryConfirmationTimeoutSecs) {
			emit deliveryConfirmationUpdated(it.key(), YaChatViewModel::DeliveryConfirmation_Timeout);
			it.remove();
		}
	}

	updateTimer();
}

void DeliveryConfirmationManager::updateTimer()
{
	if (!confirmations_.isEmpty()) {
		timer_->start();
	}
	else {
		timer_->stop();
	}
}

bool DeliveryConfirmationManager::shouldQueryWithoutTimeout(const XMPP::Jid& jid) const
{
	PsiContact* contact = account_ ? account_->findContact(jid.bare()) : 0;
	bool requestMessageReceipt = false;
	XMPP::Status::Type status = XMPP::Status::Offline;

	foreach(UserListItem* u, account_->findRelevant(jid)) {
		foreach(UserResource r, u->userResourceList()) {
			XMPP::Jid j(u->jid());
			j.setResource(r.name());
			if (jid.resource() == r.name()) {
				status = r.status().type();
				break;
			}
		}
	}

	if (!jid.resource().isEmpty()) {
		requestMessageReceipt = account_->capsManager()->features(jid).canMessageReceipts() &&
		                        (status != XMPP::Status::Offline);
	}
	else {
		foreach(UserListItem* u, account_->findRelevant(jid)) {
			foreach(UserResource r, u->userResourceList()) {
				XMPP::Jid j(u->jid());
				j.setResource(r.name());

				if (!account_->capsManager()->features(j).canMessageReceipts()) {
					requestMessageReceipt = false;
					break;
				}
			}
		}
	}

	if (requestMessageReceipt && contact && contact->authorizesToSeeStatus()) {
		return false;
	}

	return true;
}

static QString messageToString(const XMPP::Message& m)
{
	DummyStream stream;
	Stanza s = m.toStanza(&stream);
	return s.toString();
}

void DeliveryConfirmationManager::start(const XMPP::Message& m)
{
	if (logAllMessages_) {
		PsiLogger::instance()->log(QString("%1 sendMessage %2")
		                           .arg(account_->jid().full())
		                           .arg(messageToString(m)));
	}

	if (m.messageReceipt() == XMPP::ReceiptRequest) {
		QString id = m.id();
		Q_ASSERT(!id.isEmpty());
		confirmations_[id] = QDateTime::currentDateTime();
		timer_->start();

		ServerPingItem pingItem;
		pingItem.type = SPI_Message;
		pingItem.id = id;
		serverPingItems_ << pingItem;
		// debugServerPingItems("DeliveryConfirmationManager::start", &serverPingItems_);

		requestPing();
	}
}

bool DeliveryConfirmationManager::processIncomingMessage(const XMPP::Message& m)
{
	if (m.id().isEmpty())
		return false;

	if (logAllMessages_) {
		PsiLogger::instance()->log(QString("%1 recvMessage %2")
		                           .arg(account_->jid().full())
		                           .arg(messageToString(m)));
	}

	if (/* m.error().type != XMPP::Stanza::Error::Cancel && */ m.error().condition != XMPP::Stanza::Error::UndefinedCondition) {
		// QString error = tr("%1: %2", "general_error_description: detailed_error_description")
		//                 .arg(m.error().description().first)
		//                 .arg(m.error().description().second);
		QString error = m.error().description().first;
		emit messageError(m.id(), error);
		emit deliveryConfirmationUpdated(m.id(), YaChatViewModel::DeliveryConfirmation_Error);
		confirmations_.remove(m.id());
		return true;
	}
	else if (m.messageReceipt() == XMPP::ReceiptRequest) {
		// http://xmpp.org/extensions/xep-0184.html#security
		PsiContact* contact = account_->findContact(m.from().bare());
		if (contact && contact->authorized()) {
			// FIXME: must be careful sending these when we're invisible
			Message tm(m.from());
			tm.setId(m.id());
			tm.setMessageReceipt(XMPP::ReceiptReceived);
			account_->dj_sendMessage(tm, false);
		}
	}
	else if (m.messageReceipt() == XMPP::ReceiptReceived) {
		emit deliveryConfirmationUpdated(m.id(), YaChatViewModel::DeliveryConfirmation_Verified);
		if (confirmations_.contains(m.id()))
			confirmations_.remove(m.id());
	}

	updateTimer();
	return false;
}

void DeliveryConfirmationManager::accountEnabledChanged()
{
	if (account_->enabled()) {
		idlePingTimer_->start();
	}
	else {
		idlePingTimer_->stop();
		requestPingTimer_->stop();
		pingTimeoutTimer_->stop();
	}
}

void DeliveryConfirmationManager::idlePing()
{
	requestPing();
}

void DeliveryConfirmationManager::requestPing()
{
	int secs = lastPingAt_.secsTo(QDateTime::currentDateTime());
	if (qAbs(secs) < minimumPingInterval || !account_->isAvailable()) {
		requestPingTimer_->start();
		return;
	}

	lastPingAt_ = QDateTime::currentDateTime();
	requestPingTimer_->stop();

	PingTask* task = new PingTask(account_->client()->rootTask());
	connect(task, SIGNAL(finished()), SLOT(pingFinished()));
	task->ping(account_->jid().domain());
	task->go(true);

	if (!pingTimeoutTimer_->isActive()) {
		pingTimeoutTimer_->start();
	}

	ServerPingItem pingItem;
	pingItem.type = SPI_Ping;
	pingItem.id = task->id();
	serverPingItems_ << pingItem;
	// debugServerPingItems("DeliveryConfirmationManager::requestPing", &serverPingItems_);
}

void DeliveryConfirmationManager::pingTimeout()
{
	PsiLogger::instance()->log(QString("DeliveryConfirmationManager::pingTimeout() account = %1; lastPongReceivedAt_ = %2")
	                           .arg(account_->jid().full())
	                           .arg(lastPongReceivedAt_.toUTC().toString("yyyy-MM-dd HH:mm:ss")));

	reportErrorsAndResetConnection();
}

void DeliveryConfirmationManager::pingFinished()
{
	PingTask* task = static_cast<PingTask*>(sender());

	if (task->success()) {
		lastPongReceivedAt_ = QDateTime::currentDateTime();
		pingTimeoutTimer_->start();

		QList<ServerPingItem> madeItThrough;
		QMutableListIterator<ServerPingItem> it(serverPingItems_);
		while (it.hasNext()) {
			ServerPingItem item = it.next();
			it.remove();
			if (item.type != SPI_Ping) {
				madeItThrough << item;
				continue;
			}

			if (item.id != task->id()) {
				qWarning("DeliveryConfirmationManager::pingFinished: wrong ping order (got %s, expected %s)", qPrintable(task->id()), qPrintable(item.id));
				debugServerPingItems("madeItThrough", &madeItThrough);
				debugServerPingItems("serverPingItems_", &serverPingItems_);
				continue;
			}

			break;
		}

		// debugServerPingItems("madeItThrough", &madeItThrough);
		// debugServerPingItems("serverPingItems_", &serverPingItems_);
		foreach(const ServerPingItem& item, madeItThrough) {
			emit deliveryConfirmationUpdated(item.id, YaChatViewModel::DeliveryConfirmation_DeliveredToLocalServer);
		}
	}
	else {
		reportErrorsAndResetConnection();
	}
}

void DeliveryConfirmationManager::reportErrorsAndResetConnection()
{
	// debugServerPingItems("reportErrorsAndResetConnection", &serverPingItems_);
	foreach(const ServerPingItem& item, serverPingItems_) {
		if (item.type != SPI_Message)
			continue;
		emit deliveryConfirmationUpdated(item.id, YaChatViewModel::DeliveryConfirmation_ServerPingTimeout);
	}
	serverPingItems_.clear();

	emit serverPingTimeout();
}

void DeliveryConfirmationManager::debugServerPingItems(const QString& caption, const QList<ServerPingItem>* list)
{
	qWarning("=== %s", qPrintable(caption));
	foreach(const ServerPingItem& i, *list) {
		qWarning("\t[%s] %s %s",
		         qPrintable(i.createdAt.toUTC().toString("yyyy-MM-dd HH:mm:ss")),
		         i.type == SPI_Message ? "Message" : "<Ping>",
		         qPrintable(i.id));
	}
}
