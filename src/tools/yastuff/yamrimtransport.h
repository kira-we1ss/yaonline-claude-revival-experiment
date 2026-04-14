/*
 * yamrimtransport.h
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

#ifndef YAMRIMTRANSPORT_H
#define YAMRIMTRANSPORT_H

#include "yatransportmanager.h"
#include "contactupdatesmanager.h"

class YaMrimTransport : public YaTransport
{
	Q_OBJECT
public:
	YaMrimTransport(YaTransportManager* parent);

	// reimplemented
	virtual QString id() const;
	virtual bool registerTransport(const QString& login, const QString& password);
	virtual QString jid() const;
	virtual QStringList findContact(const QString& _jid) const;
	QString humanReadableJid(const QString& _jid) const;
	virtual QString transportJid(const QString& jid) const;
	virtual bool processIncomingMessage(const XMPP::Message& m);
	virtual void doSettings();

protected:

	enum State {
		State_None = 0,
		State_RemovingOldTransport,
		State_Started,
		State_Registered,

		State_Last = State_Registered
	};

private slots:
	void contactRemoveFinished(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, XMPP::Jid jid, bool success);
	bool filterContactAuth(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, XMPP::Jid jid, bool success);
	QStringList processTransportJids(const QStringList& toProcess) const;

	void setWorking(bool working);
	void startRegistration();
	void getRegistrationFormFinished();
	void setRegistrationFormFinished();
	void stop();

private:
	QStringList domains() const;
	void setState(State state);

	State state_;
	QString mrimLogin_;
	QString mrimPassword_;
};

#endif
