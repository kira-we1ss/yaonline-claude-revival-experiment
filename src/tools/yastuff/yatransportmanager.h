/*
 * yatransportmanager.h
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

#ifndef YATRANSPORTMANAGER_H
#define YATRANSPORTMANAGER_H

#include <QObject>
#include <QPointer>

class YaTransportManager;
class PsiCon;
class PsiContactList;
class PsiAccount;
class PsiContact;

#include "xmpp_status.h"
#include "xmpp_message.h"

class YaTransport : public QObject
{
	Q_OBJECT
public:
	YaTransport(YaTransportManager* parent);
	~YaTransport();

	void setConnected(bool connected);
	void remove();

	bool isSupported() const;
	bool supportsAccount(PsiAccount* account) const;
	PsiAccount* accountForContact(const QString& jid) const;

	// already exists on one of the accounts
	bool isAvailable() const;
	PsiAccount* account() const;
	void setAccount(PsiAccount* account);

	QString errorMessage() const;

	XMPP::Status::Type status() const;
	QString avatarPath(int toasterAvatarSize) const;

	virtual QString id() const = 0;
	virtual QString jid() const = 0;
	virtual QString transportLogin() const;
	virtual QString transportJid(const QString& jid) const = 0;
	virtual QStringList findContact(const QString& jid) const = 0;
	virtual QString humanReadableJid(const QString& jid) const = 0;
	virtual bool processIncomingMessage(const XMPP::Message& m);
	virtual void doSettings() {}

	virtual bool registerTransport(const QString& login, const QString& password) = 0;

signals:
	void availabilityChanged();

protected slots:
	void updateTransportLogin();

private slots:
	void addedContact(PsiContact*);
	void removedContact(PsiContact*);
	void contactUpdated();
	void updateTransportLoginFinished();

protected:
	PsiCon* controller() const;
	PsiContactList* contactList() const;
	PsiContact* transportContact() const;
	void setTransportLogin(const QString& transportLogin);

	void setErrorMessage(const QString& errorMessage);

private:
	QPointer<PsiAccount> account_;
	QPointer<PsiContact> transportContact_;
	QString errorMessage_;
	QString transportLogin_;
	bool requestingTransportLogin_;
};

class YaTransportManager : public QObject
{
	Q_OBJECT
public:
	YaTransportManager(PsiCon* parent);
	~YaTransportManager();

	PsiCon* controller() const;

	QString transportJid(const QString& jid) const;
	QStringList findContact(const QString& jid) const;
	PsiAccount* accountForContact(const QString& jid) const;
	QString humanReadableJid(const QString& jid) const;
	bool processIncomingMessage(const XMPP::Message& m);

	YaTransport* findTransport(const QString& id) const;

	QList<const YaTransport*> availableTransports() const;
	QList<const YaTransport*> transports() const;

signals:
	void availableTransportsChanged();

private slots:
	void accountCountChanged();

private:
	QList<const YaTransport*> transportsForJid(const QString& jid) const;

	QList<YaTransport*> transports_;
	QPointer<PsiCon> controller_;
};

#endif
