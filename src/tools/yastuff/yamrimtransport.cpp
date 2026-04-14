/*
 * yamrimtransport.cpp
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

#include "yamrimtransport.h"

#include <QLineEdit>
#include <QPushButton>

#include "psicon.h"
#include "psicontactlist.h"
#include "psiaccount.h"
#include "yaaccountselector.h"
#include "xmpp_jid.h"
#include "xmpp_tasks.h"
#include "yatransportmanager.h"

static QString MRIM_TRANSPORT = "mrim.ya.ru";
// static QString MRIM_TRANSPORT = "mrim.jabber.ru";
static XMPP::Jid MRIM_JID = XMPP::Jid(MRIM_TRANSPORT);

YaMrimTransport::YaMrimTransport(YaTransportManager* parent)
	: YaTransport(parent)
	, state_(State_None)
{
	controller()->contactUpdatesManager()->notifier()->registerJid(MRIM_TRANSPORT, "Mail.ru");
}

QStringList YaMrimTransport::domains() const
{
	static QStringList result;
	if (result.isEmpty()) {
		result << "corp.mail.ru";
		result << "mail.ru";
		result << "inbox.ru";
		result << "bk.ru";
		result << "list.ru";
	}
	return result;
}

QString YaMrimTransport::id() const
{
	return "MRIM";
}

bool YaMrimTransport::registerTransport(const QString& _login, const QString& password)
{
	QString login = _login;
	if (login.isEmpty())
		login = mrimLogin_;

	if (!account() || login.isEmpty() || password.isEmpty()) {
		stop();
		return false;
	}

	setWorking(true);

	setTransportLogin(mrimLogin_);
	mrimLogin_ = login;
	mrimPassword_ = password;

	controller()->contactUpdatesManager()->notifier()->registerJid(MRIM_TRANSPORT,
	        QString("%1").arg(mrimLogin_));

	PsiContact* contact = account()->findContact(MRIM_JID);
	if (contact) {
		setState(State_RemovingOldTransport);
		controller()->contactUpdatesManager()->notifier()->waitForAction(
		    ContactUpdatesNotifier::ContactRemoveFinished, account(), MRIM_JID, false,
		    this, "contactRemoveFinished");
		account()->actionRemove(MRIM_JID);
		return true;
	}

	startRegistration();
	return true;
}

QString YaMrimTransport::jid() const
{
	return MRIM_TRANSPORT;
}

QStringList YaMrimTransport::findContact(const QString& _jid) const
{
	QStringList result;

	XMPP::Jid jid(_jid);

	if (jid.node().isEmpty() && !jid.domain().isEmpty()) {
		jid.setNode(jid.domain());
		foreach(const QString& d, domains()) {
			jid.setDomain(d);
			result << jid.bare();
		}
	}
	else {
		foreach(const QString& d, domains()) {
			if (jid.domain() == d) {
				result << jid.bare();
			}
		}
	}

	return processTransportJids(result);
}

QStringList YaMrimTransport::processTransportJids(const QStringList& toProcess) const
{
	QStringList processedJids;
	foreach(const QString& str, toProcess) {
		XMPP::Jid j(str);
		QString j2 = QString("%1%%2@%3")
			.arg(j.node())
			.arg(j.domain())
			.arg(this->jid());
		processedJids << j2;
	}

	return processedJids;
}

QString YaMrimTransport::transportJid(const QString& _jid) const
{
	QStringList toProcess;

	XMPP::Jid jid(_jid);
	if (!jid.node().isEmpty() && !jid.domain().isEmpty()) {
		foreach(const QString& d, domains()) {
			if (jid.domain() == d) {
				toProcess << jid.bare();
			}
		}
	}

	QStringList result = processTransportJids(toProcess);
	if (result.isEmpty())
		return QString();
	return result.first();
}

QString YaMrimTransport::humanReadableJid(const QString& _jid) const
{
	XMPP::Jid jid(_jid);
	if (jid.domain() != this->jid())
		return QString();

	QStringList node = jid.node().split("%");
	if (node.count() != 2)
		return QString();

	return node.join("@");
}

void YaMrimTransport::stop()
{
	setState(State_None);
	setWorking(false);
}

void YaMrimTransport::setWorking(bool working)
{
	Q_UNUSED(working);
	// accountSelector_->setEnabled(!working);
	// login_->setEnabled(!working);
	// password_->setEnabled(!working);
	// button_->setText(!working ? tr("Start") : tr("Cancel"));
}

bool YaMrimTransport::filterContactAuth(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, XMPP::Jid jid, bool success)
{
	if (success) {
		if (type == ContactUpdatesNotifier::ContactAuthSubscribeRequest) {
			account->dj_auth(jid);
		}
		else if (type == ContactUpdatesNotifier::ContactAuthSubscribed ||
		         type == ContactUpdatesNotifier::ContactAuthUnsubscribed)
		{
			// ignore
		}

		return true;
	}
	return false;
}

void YaMrimTransport::contactRemoveFinished(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, XMPP::Jid jid, bool success)
{
	Q_UNUSED(type);
	Q_UNUSED(account);
	Q_UNUSED(jid);
	Q_UNUSED(success);
	if (state_ != State_RemovingOldTransport)
		return;

	startRegistration();
}

void YaMrimTransport::startRegistration()
{
	if (!account()) {
		stop();
		return;
	}
	setState(State_Started);

	XMPP::JT_Register* reg = new XMPP::JT_Register(account()->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(getRegistrationFormFinished()));
	reg->getForm(MRIM_JID);
	reg->go();
}

void YaMrimTransport::getRegistrationFormFinished()
{
	if (!account()) {
		stop();
		return;
	}

	XMPP::JT_Register* formRequestTask = (XMPP::JT_Register*)(sender());
	if (!formRequestTask->success()) {
		setErrorMessage(tr("Mail.ru transport is unavailable."));
		stop();
		return;
	}

	controller()->contactUpdatesManager()->notifier()->waitForAction(
	    ContactUpdatesNotifier::ContactAuthSubscribeRequest, account(), MRIM_JID, false,
	    this, "filterContactAuth");
	controller()->contactUpdatesManager()->notifier()->waitForAction(
	    ContactUpdatesNotifier::ContactAuthSubscribed, account(), MRIM_JID, false,
	    this, "filterContactAuth");
	controller()->contactUpdatesManager()->notifier()->waitForAction(
	    ContactUpdatesNotifier::ContactAuthUnsubscribed, account(), MRIM_JID, false,
	    this, "filterContactAuth");

	XMPP::Form form(MRIM_JID);
	form << XMPP::FormField("email", mrimLogin_);
	form << XMPP::FormField("password", mrimPassword_);
	XMPP::JT_Register* reg = new XMPP::JT_Register(account()->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(setRegistrationFormFinished()));
	reg->setForm(form);
	reg->go();
}

void YaMrimTransport::setRegistrationFormFinished()
{
	XMPP::JT_Register* setFormTask = (XMPP::JT_Register*)(sender());
	if (!setFormTask->success()) {
		setTransportLogin(mrimLogin_);
		setErrorMessage(tr("Invalid Mail.ru Login / Password."));
		stop();
		return;
	}

	setState(State_Registered);
	stop();
	setErrorMessage(QString());
	updateTransportLogin();
}

void YaMrimTransport::setState(State state)
{
	state_ = state;
}

bool YaMrimTransport::processIncomingMessage(const XMPP::Message& m)
{
	if (m.error().condition != XMPP::Stanza::Error::UndefinedCondition) {
		QString error = m.error().description().first;
		setErrorMessage(error);
		return true;
	}

	return false;
}

void YaMrimTransport::doSettings()
{
	if (!account())
		return;
	account()->actionExecuteCommand("mrim.ya.ru", "mail");
}
