/*
 * yatransportmanager.cpp
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

#include "yatransportmanager.h"

#include <QStringList>

#include "xmpp_jid.h"
#include "psicon.h"
#include "psicontactlist.h"
#include "psiaccount.h"
#include "psicontact.h"
#include "yavisualutil.h"
#include "xmpp_tasks.h"

#include "yamrimtransport.h"
#include "yaj2jtransport.h"

//----------------------------------------------------------------------------
// YaTransport
//----------------------------------------------------------------------------

YaTransport::YaTransport(YaTransportManager* parent)
	: QObject(parent)
	, requestingTransportLogin_(false)
{
}

YaTransport::~YaTransport()
{
}

PsiCon* YaTransport::controller() const
{
	return static_cast<YaTransportManager*>(parent())->controller();
}

PsiContactList* YaTransport::contactList() const
{
	return controller() ? controller()->contactList() : 0;
}

bool YaTransport::isSupported() const
{
	if (account_)
		return true;
	return accountForContact(jid()) != 0;
}

bool YaTransport::supportsAccount(PsiAccount* account) const
{
	QStringList jidParts = jid().split(".");
	jidParts.takeFirst();
	QString j = jidParts.join(".");
	return account->jid().domain() == j;
}

PsiAccount* YaTransport::accountForContact(const QString& jid) const
{
	if (contactList()) {
		XMPP::Jid j(jid);
		foreach(PsiAccount* a, contactList()->enabledAccounts()) {
			if (a->findContact(j.domain())) {
				return a;
			}
		}
	}
	return 0;
}

PsiAccount* YaTransport::account() const
{
	return account_;
}

void YaTransport::setAccount(PsiAccount* account)
{
	blockSignals(true);

	if (account_) {
		disconnect(account_, SIGNAL(addedContact(PsiContact*)), this, SLOT(addedContact(PsiContact*)));
		disconnect(account_, SIGNAL(removedContact(PsiContact*)), this, SLOT(removedContact(PsiContact*)));
		disconnect(account_, SIGNAL(updatedActivity()), this, SLOT(updateTransportLogin()));
	}
	account_ = account;
	if (account_) {
		connect(account_, SIGNAL(addedContact(PsiContact*)), this, SLOT(addedContact(PsiContact*)));
		connect(account_, SIGNAL(removedContact(PsiContact*)), this, SLOT(removedContact(PsiContact*)));
		connect(account_, SIGNAL(updatedActivity()), this, SLOT(updateTransportLogin()));
	}

	removedContact(transportContact_);
	addedContact(account_->findContact(jid()));

	blockSignals(false);
	emit availabilityChanged();
}

void YaTransport::addedContact(PsiContact* contact)
{
	if (contact && contact->jid() == jid()) {
		transportContact_ = contact;
		if (transportContact_) {
			connect(transportContact_, SIGNAL(updated()), this, SLOT(contactUpdated()));
			updateTransportLogin();
		}
		emit availabilityChanged();
	}
}

void YaTransport::removedContact(PsiContact* contact)
{
	if (contact && contact->jid() == jid()) {
		if (transportContact_) {
			disconnect(transportContact_, SIGNAL(updated()), this, SLOT(contactUpdated()));
		}
		transportContact_ = 0;
		transportLogin_ = QString();
		requestingTransportLogin_ = false;
		emit availabilityChanged();
	}
}

void YaTransport::updateTransportLogin()
{
	if (!account()->isAvailable() || !transportContact_ || requestingTransportLogin_)
		return;
	requestingTransportLogin_ = true;
	XMPP::JT_Register* reg = new XMPP::JT_Register(account()->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(updateTransportLoginFinished()));
	reg->getForm(jid());
	reg->go();
}

void YaTransport::updateTransportLoginFinished()
{
	XMPP::JT_Register* formRequestTask = (XMPP::JT_Register*)(sender());
	transportLogin_ = QString();
	requestingTransportLogin_ = false;
	if (formRequestTask->success()) {
		XMPP::Form form = formRequestTask->form();
		bool found = false;
		foreach(const XMPP::FormField& f, form) {
			if (f.type() == XMPP::FormField::email) {
				found = true;
				transportLogin_ = f.value();
				break;
			}
		}

		if (!found) {
			XMPP::Jid j;
			bool usernameFound = false;
			bool serverFound = false;
			foreach(const XMPP::XData::Field& f, formRequestTask->xdata().fields()) {
				if (f.var() == "username") {
					usernameFound = true;
					j.setNode(f.value().join(""));
				}
				else if (f.var() == "server") {
					serverFound = true;
					j.setDomain(f.value().join(""));
				}
			}

			if (usernameFound && serverFound) {
				transportLogin_ = j.full();
			}
		}
	}
	emit availabilityChanged();
}

void YaTransport::contactUpdated()
{
	PsiContact* contact = static_cast<PsiContact*>(sender());
	if (contact != transportContact_)
		return;

	if (transportLogin_.isEmpty()) {
		updateTransportLogin();
	}
	emit availabilityChanged();
}

QString YaTransport::errorMessage() const
{
	return errorMessage_;
}

void YaTransport::setErrorMessage(const QString& errorMessage)
{
	errorMessage_ = errorMessage;
	emit availabilityChanged();
}

bool YaTransport::isAvailable() const
{
	return !transportContact_.isNull();
}

XMPP::Status::Type YaTransport::status() const
{
	return transportContact_ ? transportContact_->status().type() : XMPP::Status::Offline;
}

QString YaTransport::avatarPath(int toasterAvatarSize) const
{
	if (!transportContact_)
		return QString();

	return Ya::VisualUtil::scaledAvatarPath(transportContact_->account(), transportContact_->jid(), toasterAvatarSize);
}

PsiContact* YaTransport::transportContact() const
{
	return transportContact_;
}

void YaTransport::setConnected(bool connected)
{
	if (!transportContact_)
		return;

	XMPP::Status status;
	if (connected)
		status = transportContact_->account()->status();
	else
		status = XMPP::Status(XMPP::Status::Offline);
	transportContact_->account()->actionAgentSetStatus(transportContact_->jid(), status);
}

void YaTransport::remove()
{
	setErrorMessage(QString());
	if (transportContact_) {
		transportContact_->account()->actionRemove(jid());
	}
	else {
		transportLogin_ = QString();
		requestingTransportLogin_ = false;
		emit availabilityChanged();
	}
}

QString YaTransport::transportLogin() const
{
	return transportLogin_;
}

void YaTransport::setTransportLogin(const QString& transportLogin)
{
	transportLogin_ = transportLogin;
	emit availabilityChanged();
}

bool YaTransport::processIncomingMessage(const XMPP::Message& m)
{
	Q_UNUSED(m);
	return false;
}

//----------------------------------------------------------------------------
// YaTransportManager
//----------------------------------------------------------------------------

YaTransportManager::YaTransportManager(PsiCon* parent)
	: QObject(parent)
	, controller_(parent)
{
	transports_ << new YaMrimTransport(this);
	transports_ << new YaJ2jTransport(this);

	foreach(YaTransport* t, transports_) {
		connect(t, SIGNAL(availabilityChanged()), this, SIGNAL(availableTransportsChanged()), Qt::QueuedConnection);
	}

	connect(controller_->contactList(), SIGNAL(accountCountChanged()), this, SLOT(accountCountChanged()));
	accountCountChanged();
}

YaTransportManager::~YaTransportManager()
{
}

PsiCon* YaTransportManager::controller() const
{
	return controller_;
}

QList<const YaTransport*> YaTransportManager::transportsForJid(const QString& jid) const
{
	Q_UNUSED(jid);
	QList<const YaTransport*> result;
	foreach(const YaTransport* t, transports_) {
		if (!t->isSupported())
			continue;
		result << t;
	}
	return result;
}

QList<const YaTransport*> YaTransportManager::availableTransports() const
{
	QList<const YaTransport*> result;
	foreach(const YaTransport* t, transports_) {
		if (!t->isAvailable())
			continue;
		result << t;
	}
	return result;
}

QList<const YaTransport*> YaTransportManager::transports() const
{
	QList<const YaTransport*> result;
	foreach(const YaTransport* t, transports_) {
		result << t;
	}
	return result;
}

QString YaTransportManager::transportJid(const QString& jid) const
{
	QString result;
	foreach(const YaTransport* t, transportsForJid(jid)) {
		result = t->transportJid(jid);
	}
	return result;
}

QStringList YaTransportManager::findContact(const QString& jid) const
{
	QStringList result;
	foreach(const YaTransport* t, transportsForJid(jid)) {
		result += t->findContact(jid);
	}
	return result;
}

PsiAccount* YaTransportManager::accountForContact(const QString& jid) const
{
	foreach(const YaTransport* t, transportsForJid(jid)) {
		PsiAccount* acc = t->accountForContact(jid);
		if (acc)
			return acc;
	}
	return 0;
}

QString YaTransportManager::humanReadableJid(const QString& jid) const
{
	foreach(const YaTransport* t, transportsForJid(jid)) {
		QString j = t->humanReadableJid(jid);
		if (!j.isEmpty())
			return j;
	}
	return QString();
}

void YaTransportManager::accountCountChanged()
{
	foreach(YaTransport* t, transports_) {
		foreach(PsiAccount* acc, controller_->contactList()->enabledAccounts()) {
			if (t->supportsAccount(acc)) {
				t->setAccount(acc);
				break;
			}
		}
	}
}

YaTransport* YaTransportManager::findTransport(const QString& id) const
{
	foreach(YaTransport* t, transports_) {
		if (t->id() == id)
			return t;
	}
	return 0;
}

bool YaTransportManager::processIncomingMessage(const XMPP::Message& m)
{
	foreach(const YaTransport* t, transportsForJid(m.from().bare())) {
		bool result = const_cast<YaTransport*>(t)->processIncomingMessage(m);
		if (result) {
			return result;
		}
	}
	return false;
}
