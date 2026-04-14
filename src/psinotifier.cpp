/*
 * psinotifier.cpp
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

#include "psinotifier.h"

#include <QStringList>
#include <QPixmap>

#include "common.h"
#include "psiaccount.h"
#include "avatars.h"
#include "psievent.h"
#include "userlist.h"

#ifdef YAPSI
#include "yacommon.h"
#include "psioptions.h"
#include "psicontact.h"
#include "yavisualutil.h"
#endif

#include "psinotifierbase.h"
#if defined(Q_WS_MAC) && defined(HAVE_GROWL)
#include "psigrowlnotifier.h"
#endif

#if defined(Q_WS_X11) && defined(USE_DBUS)
#include "psidbusnotifier.h"
#endif

PsiNotifier::PsiNotifier()
	: QObject()
	, psiNotifier_(0)
{
	// Initialize all notifications
	QStringList nots;
	nots << QObject::tr("Contact becomes Available");
	nots << QObject::tr("Contact becomes Unavailable");
	nots << QObject::tr("Contact changes Status");
	nots << QObject::tr("Incoming Message");
	nots << QObject::tr("Incoming Headline");
	nots << QObject::tr("Incoming File");
#ifdef YAPSI
	nots << QObject::tr("Mood Changed");
	nots << QObject::tr("Connection Successful");
	nots << QObject::tr("Connection Error");
#endif

	// Initialize default notifications
	QStringList defaults;
#ifndef YAPSI
	defaults << QObject::tr("Contact becomes Available");
#endif
	defaults << QObject::tr("Incoming Message");
	defaults << QObject::tr("Incoming Headline");
	defaults << QObject::tr("Incoming File");
#ifdef YAPSI
	defaults << QObject::tr("Mood Changed");
	defaults << QObject::tr("Connection Successful");
	defaults << QObject::tr("Connection Error");
#endif

	// Register with Growl
	QString appName = "Psi";
#ifdef YAPSI
	appName = QObject::tr("Ya.Online");
#endif

#if defined(Q_WS_MAC) && defined(HAVE_GROWL)
	psiNotifier_ = new PsiGrowlNotifier(this, nots, defaults, appName);
#elif defined(Q_WS_X11) && defined(USE_DBUS)
	psiNotifier_ = new PsiDbusNotifier(this, appName);
#endif
}

PsiNotifier::~PsiNotifier()
{
}

PsiNotifier* PsiNotifier::instance()
{
	static PsiNotifier* instance = 0;
	if (!instance) {
		instance = new PsiNotifier();
	}
	return instance;
}

bool PsiNotifier::isNative() const
{
	return psiNotifier_;
}

#ifdef YAPSI
static QPixmap getAvatar(PsiAccount* account, const XMPP::Jid& jid)
{
	Q_ASSERT(account);
	QPixmap result = account->avatarFactory()->getAvatar(jid.bare());
	if (result.isNull()) {
		PsiContact* contact = account->findContact(jid);
		XMPP::VCard::Gender gender = contact ? contact->gender() : XMPP::VCard::UnknownGender;
		result = Ya::VisualUtil::noAvatarPixmapFileName(gender);
	}
	return result;
}
#endif

void PsiNotifier::popup(PsiAccount* account, PsiPopup::PopupType type, const XMPP::Jid& jid, const XMPP::Resource& r, const UserListItem* uli, PsiEvent* event)
{
	if (!psiNotifier_)
		return;

	QString name;
	QString title, desc, contact;
	QString statusTxt = status2txt(makeSTATUS(r.status()));
	QString statusMsg = r.status().status();
#ifdef YAPSI
	QPixmap icon = getAvatar(account, jid);
#else
	QPixmap icon = account->avatarFactory()->getAvatar(jid.bare());
#endif
	if (uli) {
		contact = uli->name();
	}
	else if (event->type() == PsiEvent::Auth) {
		contact = ((AuthEvent*) event)->nick();
	}
	else if (event->type() == PsiEvent::Message) {
		contact = ((MessageEvent*) event)->nick();
	}

	if (contact.isEmpty())
		contact = jid.bare();

	// Default value for the title
	title = contact;

	switch(type) {
		case PsiPopup::AlertOnline:
#ifdef YAPSI
			return;
#endif
			name = QObject::tr("Contact becomes Available");
			title = QString("%1 (%2)").arg(contact).arg(statusTxt);
			desc = statusMsg;
			//icon = PsiIconset::instance()->statusPQString(jid, r.status());
			break;
		case PsiPopup::AlertOffline:
#ifdef YAPSI
			return;
#endif
			name = QObject::tr("Contact becomes Unavailable");
			title = QString("%1 (%2)").arg(contact).arg(statusTxt);
			desc = statusMsg;
			//icon = PsiIconset::instance()->statusPQString(jid, r.status());
			break;
		case PsiPopup::AlertStatusChange:
#ifdef YAPSI
			return;
#endif
			name = QObject::tr("Contact changes Status");
			title = QString("%1 (%2)").arg(contact).arg(statusTxt);
			desc = statusMsg;
			//icon = PsiIconset::instance()->statusPQString(jid, r.status());
			break;
		case PsiPopup::AlertMessage:
		case PsiPopup::AlertChat: {
			name = QObject::tr("Incoming Message");
			if (type == PsiPopup::AlertMessage) {
#ifdef YAPSI
				title = QObject::tr("%1").arg(contact);
#else
				title = QObject::tr("%1 says:").arg(contact);
#endif
			}
			MessageEvent* messageEvent = dynamic_cast<MessageEvent *>(event);
			Q_ASSERT(messageEvent);
			desc = messageEvent->description();
#ifdef YAPSI
			if (!messageEvent->isForcedDescription()) {
				desc = Ya::messageNotifierText(desc);
			}

			if (messageEvent->multiContactCount() > 1) {
				contact = Ya::multipleContactName(messageEvent->multiContactCount());
				title = contact;
				icon = Ya::VisualUtil::noAvatarPixmap(XMPP::VCard::MultipleGender);
			}

			if (!PsiOptions::instance()->getOption("options.ya.popups.message.enable").toBool()) {
				return;
			}
#endif
			break;
		}
		case PsiPopup::AlertHeadline: {
			name = QObject::tr("Incoming Headline");
			const Message* jmessage = &((MessageEvent *)event)->message();
			if ( !jmessage->subject().isEmpty())
				title = jmessage->subject();
			desc = jmessage->body();
			//icon = IconsetFactory::iconPQString("psi/headline");
			break;
		}
		case PsiPopup::AlertFile:
			name = QObject::tr("Incoming File");
			desc = QObject::tr("[Incoming File]");
			//icon = IconsetFactory::iconPQString("psi/file");
			break;
		default:
			break;
	}

	psiNotifier_->notify(account, jid, name, title, desc, icon);
}

#ifdef YAPSI
void PsiNotifier::moodChanged(PsiAccount* account, const XMPP::Jid& jid, const QString& name, const QString& mood)
{
	if (!psiNotifier_)
		return;

	QString notifyName = QObject::tr("Mood Changed");
	QString title = tr("%1 changed mood to:").arg(name);
	QString desc = mood;
	QPixmap icon = getAvatar(account, jid);

	psiNotifier_->notify(account, jid, notifyName, title, desc, icon);
}

void PsiNotifier::showSuccessfullyConnectedToaster(PsiAccount* account)
{
	if (!psiNotifier_)
		return;

	if (!PsiOptions::instance()->getOption("options.ya.popups.connection.enable").toBool()) {
		return;
	}

	QString notifyName = QObject::tr("Connection Successful");
	QString title = account->jid().bare();
	QString desc = tr("Successfully connected to server.");
	QPixmap icon = getAvatar(account, account->jid());

	psiNotifier_->notify(account, XMPP::Jid(), notifyName, title, desc, icon);
}

void PsiNotifier::showConnectionErrorToaster(PsiAccount* account, const QString& error)
{
	if (!psiNotifier_)
		return;

	if (!PsiOptions::instance()->getOption("options.ya.popups.connection.enable").toBool()) {
		return;
	}

	QString notifyName = QObject::tr("Connection Error");
	QString title = account->jid().bare();
	QString desc = tr("%1").arg(error);
	QPixmap icon = getAvatar(account, account->jid());

	psiNotifier_->notify(account, XMPP::Jid(), notifyName, title, desc, icon);
}
#endif // YAPSI
