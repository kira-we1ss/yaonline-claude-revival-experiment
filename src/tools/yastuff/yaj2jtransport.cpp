/*
 * yaj2jtransport.cpp
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

#include "yaj2jtransport.h"

#include <QLineEdit>
#include <QPushButton>

#include "psicon.h"
#include "psicontactlist.h"
#include "psiaccount.h"
#include "yaaccountselector.h"
#include "xmpp_jid.h"
#include "xmpp_tasks.h"
#include "yatransportmanager.h"

static QString J2J_TRANSPORT = "j2j.ya.ru";
static XMPP::Jid J2J_JID = XMPP::Jid(J2J_TRANSPORT);

YaJ2jTransport::YaJ2jTransport(YaTransportManager* parent)
	: YaTransport(parent)
	, state_(State_None)
{
	controller()->contactUpdatesManager()->notifier()->registerJid(J2J_TRANSPORT, "J2J");
}

QStringList YaJ2jTransport::domains() const
{
	QStringList result;
	if (!j2jLogin_.isEmpty()) {
		XMPP::Jid j(j2jLogin_);
		result << j.domain();
	}
	return result;
}

QString YaJ2jTransport::id() const
{
	return "J2J";
}

bool YaJ2jTransport::registerTransport(const QString& _login, const QString& password)
{
	QString login = _login;
	if (login.isEmpty())
		login = j2jLogin_;

	if (!account() || login.isEmpty() || password.isEmpty()) {
		stop();
		return false;
	}

	setWorking(true);

	setTransportLogin(j2jLogin_);
	j2jLogin_ = login;
	j2jPassword_ = password;

	controller()->contactUpdatesManager()->notifier()->registerJid(J2J_TRANSPORT,
	        QString("%1").arg(j2jLogin_));

	PsiContact* contact = account()->findContact(J2J_JID);
	if (contact) {
		setState(State_RemovingOldTransport);
		controller()->contactUpdatesManager()->notifier()->waitForAction(
		    ContactUpdatesNotifier::ContactRemoveFinished, account(), J2J_JID, false,
		    this, "contactRemoveFinished");
		account()->actionRemove(J2J_JID);
		return true;
	}

	startRegistration();
	return true;
}

QString YaJ2jTransport::jid() const
{
	return J2J_TRANSPORT;
}

QStringList YaJ2jTransport::findContact(const QString& _jid) const
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

QStringList YaJ2jTransport::processTransportJids(const QStringList& toProcess) const
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

QString YaJ2jTransport::transportJid(const QString& _jid) const
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

QString YaJ2jTransport::humanReadableJid(const QString& _jid) const
{
	XMPP::Jid jid(_jid);
	if (jid.domain() != this->jid())
		return QString();

	QStringList node = jid.node().split("%");
	if (node.count() != 2)
		return QString();

	return node.join("@");
}

void YaJ2jTransport::stop()
{
	setState(State_None);
	setWorking(false);
}

void YaJ2jTransport::setWorking(bool working)
{
	Q_UNUSED(working);
	// accountSelector_->setEnabled(!working);
	// login_->setEnabled(!working);
	// password_->setEnabled(!working);
	// button_->setText(!working ? tr("Start") : tr("Cancel"));
}

bool YaJ2jTransport::filterContactAuth(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, XMPP::Jid jid, bool success)
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

void YaJ2jTransport::contactRemoveFinished(ContactUpdatesNotifier::ContactUpdateActionType type, PsiAccount* account, XMPP::Jid jid, bool success)
{
	Q_UNUSED(type);
	Q_UNUSED(account);
	Q_UNUSED(jid);
	Q_UNUSED(success);
	if (state_ != State_RemovingOldTransport)
		return;

	startRegistration();
}

void YaJ2jTransport::startRegistration()
{
	if (!account()) {
		stop();
		return;
	}
	setState(State_Started);

	XMPP::JT_Register* reg = new XMPP::JT_Register(account()->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(getRegistrationFormFinished()));
	reg->getForm(J2J_JID);
	reg->go();
}

void YaJ2jTransport::getRegistrationFormFinished()
{
	if (!account()) {
		stop();
		return;
	}

	XMPP::JT_Register* formRequestTask = (XMPP::JT_Register*)(sender());
	if (!formRequestTask->success()) {
		setErrorMessage(tr("J2J transport is unavailable."));
		stop();
		return;
	}

	controller()->contactUpdatesManager()->notifier()->waitForAction(
	    ContactUpdatesNotifier::ContactAuthSubscribeRequest, account(), J2J_JID, false,
	    this, "filterContactAuth");
	controller()->contactUpdatesManager()->notifier()->waitForAction(
	    ContactUpdatesNotifier::ContactAuthSubscribed, account(), J2J_JID, false,
	    this, "filterContactAuth");
	controller()->contactUpdatesManager()->notifier()->waitForAction(
	    ContactUpdatesNotifier::ContactAuthUnsubscribed, account(), J2J_JID, false,
	    this, "filterContactAuth");

	XMPP::Jid j(j2jLogin_);

	XMPP::JT_Register* reg = new XMPP::JT_Register(account()->client()->rootTask());
	connect(reg, SIGNAL(finished()), SLOT(setRegistrationFormFinished()));

	XMPP::XData form;
	form.setType(XMPP::XData::Data_Submit);
	form.addField(XMPP::XData::Field::Field_TextSingle, "username", j.node());
	form.addField(XMPP::XData::Field::Field_TextSingle, "server", j.domain());
	form.addField(XMPP::XData::Field::Field_TextPrivate, "password", j2jPassword_);
	form.addField(XMPP::XData::Field::Field_TextSingle, "domain", QString());
	form.addField(XMPP::XData::Field::Field_TextSingle, "port", QString::number(5222));
	form.addField(XMPP::XData::Field::Field_Boolean, "import_roster", QString::number(1));
	form.addField(XMPP::XData::Field::Field_Boolean, "remove_from_roster", QString::number(0));

	reg->setForm(J2J_JID, form);
	reg->go();
}

void YaJ2jTransport::setRegistrationFormFinished()
{
	XMPP::JT_Register* setFormTask = (XMPP::JT_Register*)(sender());
	if (!setFormTask->success()) {
		setTransportLogin(j2jLogin_);
		setErrorMessage(tr("Invalid Login / Password."));
		stop();
		return;
	}

	setState(State_Registered);
	stop();
	setErrorMessage(QString());
	updateTransportLogin();
}

void YaJ2jTransport::setState(State state)
{
	state_ = state;
}

bool YaJ2jTransport::processIncomingMessage(const XMPP::Message& m)
{
	if (m.error().condition != XMPP::Stanza::Error::UndefinedCondition) {
		QString error = m.error().description().first;
		setErrorMessage(error);
		return true;
	}

	return false;
}

void YaJ2jTransport::doSettings()
{
	if (!account())
		return;
	// account()->actionExecuteCommand("j2j.ya.ru", "mail");
}
