/*
 * deliveryconfirmationmanager.h
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

#ifndef DELIVERYCONFIRMATIONMANAGER_H
#define DELIVERYCONFIRMATIONMANAGER_H

#include <QObject>
#include <QHash>

#include "xmpp_message.h"
#include "yachatviewmodel.h"

class PsiAccount;
class QTimer;

class DeliveryConfirmationManager : public QObject
{
	Q_OBJECT
public:
	DeliveryConfirmationManager(PsiAccount* account);
	~DeliveryConfirmationManager();

	void start(const XMPP::Message& m);
	bool processIncomingMessage(const XMPP::Message& m);

	bool shouldQueryWithoutTimeout(const XMPP::Jid& jid) const;

signals:
	void messageError(const QString& id, const QString& error);
	void deliveryConfirmationUpdated(const QString& id, YaChatViewModel::DeliveryConfirmationType deliveryConfirmation);
	void serverPingTimeout();

private slots:
	void timeout();

	void accountEnabledChanged();

	void idlePing();
	void requestPing();
	void pingFinished();
	void pingTimeout();
	void reportErrorsAndResetConnection();

private:
	enum ServerPingItemType {
		SPI_Message = 0,
		SPI_Ping
	};

	struct ServerPingItem {
		ServerPingItem()
			: createdAt(QDateTime::currentDateTime())
		{}

		QDateTime createdAt;

		ServerPingItemType type;
		QString id;
	};

	PsiAccount* account_;
	QTimer* timer_;
	QHash<QString, QDateTime> confirmations_;
	bool logAllMessages_;

	QDateTime lastPingAt_;
	QDateTime lastPongReceivedAt_;
	QTimer* requestPingTimer_;
	QTimer* idlePingTimer_;
	QTimer* pingTimeoutTimer_;
	QList<ServerPingItem> serverPingItems_;
	void debugServerPingItems(const QString& caption, const QList<ServerPingItem>* list);

	void updateTimer();
};

#endif
