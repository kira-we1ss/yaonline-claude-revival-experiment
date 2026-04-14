/*
 * yamucmanager.cpp
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

#include "yamucmanager.h"

#include "psicontactlist.h"
#include "psiaccount.h"
#include "psicon.h"
#include "xmpp_tasks.h"
#include "xmpp_discoinfotask.h"

YaMucManager::YaMucManager(PsiCon* controller)
	: QObject(controller)
	, controller_(controller)
{
	mucServersPre_ << "conference.ya.ru";

	mucServersPost_ << "conference.jabber.ru";
	mucServersPost_ << "conference.qip.ru";

	connect(controller_->contactList(), SIGNAL(accountCountChanged()), SLOT(accountCountChanged()));
	accountCountChanged();
}

YaMucManager::~YaMucManager()
{
}

QStringList YaMucManager::mucServers() const
{
	QStringList result;
	if (!controller_->contactList()->accountsLoaded())
		return result;

	result << mucServersPre_;

	if (controller_->contactList()->yandexTeamAccount() &&
	    controller_->contactList()->yandexTeamAccount()->enabled())
	{
		result << "conference.yandex-team.ru";
	}

	QHashIterator<QString, QStringList> it(discoveredServers_);
	while (it.hasNext()) {
		it.next();

		if (it.key() == "yandex-team.ru")
			continue;

		bool accountActive = false;
		foreach(PsiAccount* account, controller_->contactList()->enabledAccounts()) {
			if (account->jid().domain() == it.key()) {
				accountActive = true;
				break;
			}
		}

		if (!accountActive)
			continue;

		foreach(const QString& muc, it.value()) {
			if (mucServersPre_.contains(muc) ||
			    mucServersPost_.contains(muc))
			{
				continue;
			}

			result << muc;
		}
	}

	result << mucServersPost_;
	return result;
}

void YaMucManager::accountCountChanged()
{
	foreach(PsiAccount* account, controller_->contactList()->accounts()) {
		disconnect(account, SIGNAL(rosterRequestFinished()), this, SLOT(rosterRequestFinished()));
		connect(account,    SIGNAL(rosterRequestFinished()), this, SLOT(rosterRequestFinished()));

		if (account->isAvailable()) {
			checkAccountForAvailableMucServers(account);
		}
	}
}

void YaMucManager::rosterRequestFinished()
{
	PsiAccount* account = static_cast<PsiAccount*>(sender());
	if (account->isAvailable()) {
		checkAccountForAvailableMucServers(account);
	}
}

void YaMucManager::checkAccountForAvailableMucServers(PsiAccount* account)
{
	Q_ASSERT(account->isAvailable());
	QString domain = account->jid().domain();
	if (discoveredServers_.contains(domain)) {
		return;
	}

	discoveredServers_[domain] = QStringList();
	XMPP::JT_DiscoItems* itemsTask = new XMPP::JT_DiscoItems(account->client()->rootTask());
	connect(itemsTask, SIGNAL(finished()), SLOT(discoItemsFinished()));
	itemsTask->get(domain);
	itemsTask->go();
}

void YaMucManager::discoItemsFinished()
{
	XMPP::JT_DiscoItems* itemsTask = static_cast<XMPP::JT_DiscoItems*>(sender());
	QString domain = itemsTask->jid().domain();
	if (!itemsTask->success()) {
		qWarning("YaMucManager::discoItemsFinished(): Unable to get items for %s", qPrintable(domain));
		discoveredServers_.remove(domain);
		return;
	}

	foreach(const XMPP::DiscoItem& item, itemsTask->items()) {
		if (item.identities().isEmpty()) {
			XMPP::DiscoInfoTask* infoTask = new XMPP::DiscoInfoTask(itemsTask->parent());
			infoTask->setProperty("domain", domain);

			connect(infoTask, SIGNAL(finished()), SLOT(discoInfoFinished()));
			infoTask->get(item.jid(), item.node());
			infoTask->go();
			continue;
		}

		processDiscoItemIdentities(domain, item);
	}
}

void YaMucManager::discoInfoFinished()
{
	XMPP::DiscoInfoTask* infoTask = static_cast<XMPP::DiscoInfoTask*>(sender());
	QString domain = infoTask->property("domain").toString();
	if (!infoTask->success()) {
		qWarning("YaMucManager::discoInfoFinished(): Unable to get info for (%s) %s", qPrintable(domain), qPrintable(infoTask->jid().full()));
		return;
	}

	processDiscoItemIdentities(domain, infoTask->item());
}

void YaMucManager::processDiscoItemIdentities(const QString& domain, const XMPP::DiscoItem& discoItem)
{
	foreach(const XMPP::DiscoItem::Identity& i, discoItem.identities()) {
		if (i.category == QLatin1String("conference") && i.type == QLatin1String("text")) {
			if (!discoveredServers_.contains(domain))
				discoveredServers_[domain] = QStringList();
			if (!discoveredServers_[domain].contains(discoItem.jid().full()))
				discoveredServers_[domain] << discoItem.jid().full();

			emit mucServersChanged();
		}
	}
}
