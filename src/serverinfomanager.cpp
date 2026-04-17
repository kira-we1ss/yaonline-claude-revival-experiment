/*
 * serverinfomanager.cpp
 * Copyright (C) 2006  Remko Troncon
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

#include "serverinfomanager.h"
#include "xmpp_tasks.h"
#include "xmpp_discoinfotask.h"
#include <QDebug>

using namespace XMPP;

ServerInfoManager::ServerInfoManager(Client* client) : client_(client)
{
	deinitialize();
	connect(client_, SIGNAL(rosterRequestFinished(bool, int, const QString &)), SLOT(initialize()));
	connect(client_, SIGNAL(disconnected()), SLOT(deinitialize()));
}

void ServerInfoManager::reset()
{
	hasPEP_ = false;
	multicastService_ = QString();
	hasMAM_ = false;
	hasCarbons_ = false;
	hasHttpUpload_ = false;
	httpUploadService_ = QString();
}

void ServerInfoManager::initialize()
{
	// Query server disco#info for feature flags (PEP, MAM, Carbons, etc.)
	JT_DiscoInfo *jt = new JT_DiscoInfo(client_->rootTask());
	connect(jt, SIGNAL(finished()), SLOT(disco_finished()));
	jt->get(client_->jid().domain());
	jt->go(true);

	// Query server disco#items to discover sub-services (HTTP Upload, etc.)
	JT_DiscoItems *items = new JT_DiscoItems(client_->rootTask());
	connect(items, SIGNAL(finished()), SLOT(items_finished()));
	items->get(client_->jid().domain());
	items->go(true);
}

void ServerInfoManager::deinitialize()
{
	reset();
	emit featuresChanged();
}

const QString& ServerInfoManager::multicastService() const
{
	return multicastService_;
}

bool ServerInfoManager::hasPEP() const
{
	return hasPEP_;
}

bool ServerInfoManager::hasMAM() const
{
	return hasMAM_;
}

bool ServerInfoManager::hasCarbons() const
{
	return hasCarbons_;
}

bool ServerInfoManager::hasHttpUpload() const
{
	return hasHttpUpload_;
}

QString ServerInfoManager::httpUploadService() const
{
	return httpUploadService_;
}

void ServerInfoManager::disco_finished()
{
	JT_DiscoInfo *jt = (JT_DiscoInfo *)sender();
	if (jt->success()) {
		// Features
		Features f = jt->item().features();
		if (f.canMulticast())
			multicastService_ = client_->jid().domain();
		// TODO: Remove this, this is legacy
		if (f.test(QStringList("http://jabber.org/protocol/pubsub#pep")))
			hasPEP_ = true;

		// Layer 5 XEP feature detection in server's disco#info
		if (f.test(QStringList("urn:xmpp:mam:2")))
			hasMAM_ = true;
		if (f.test(QStringList("urn:xmpp:carbons:2")))
			hasCarbons_ = true;

		// Identities
		DiscoItem::Identities is = jt->item().identities();
		foreach(DiscoItem::Identity i, is) {
			if (i.category == "pubsub" && i.type == "pep")
				hasPEP_ = true;
		}

		emit featuresChanged();
	}
}

void ServerInfoManager::items_finished()
{
	JT_DiscoItems *jt = (JT_DiscoItems *)sender();
	if (!jt->success()) {
		qDebug() << "[ServerInfo] disco#items failed on" << client_->jid().domain();
		return;
	}

	qDebug() << "[ServerInfo] disco#items on" << client_->jid().domain()
	         << "returned" << jt->items().size() << "items";

	// For each discovered item, query its disco#info to detect upload services
	foreach(const DiscoItem& item, jt->items()) {
		qDebug() << "[ServerInfo]   -> querying sub-component:" << item.jid().full();
		DiscoInfoTask *infoTask = new DiscoInfoTask(client_->rootTask());
		infoTask->setProperty("itemJid", item.jid().full());
		connect(infoTask, SIGNAL(finished()), SLOT(item_info_finished()));
		infoTask->get(item.jid(), item.node());
		infoTask->go(true);
	}
}

void ServerInfoManager::item_info_finished()
{
	DiscoInfoTask *jt = (DiscoInfoTask *)sender();
	if (!jt->success()) {
		qDebug() << "[ServerInfo] disco#info failed for"
		         << jt->property("itemJid").toString();
		return;
	}

	// XEP-0363 HTTP File Upload detection.
	//
	// Primary check: feature string. Prosody, ejabberd and most modern servers
	// advertise urn:xmpp:http:upload:0 as a disco feature on the upload
	// component but do NOT necessarily include the optional store/file identity.
	// Also accept the legacy namespace without the :0 suffix for older deployments.
	//
	// Fallback check: identity category="store" type="file" — required by the
	// spec but rarely advertised in practice.
	Features feats = jt->item().features();
	bool isUploadService = feats.test(
		QStringList() << QLatin1String("urn:xmpp:http:upload:0")
		              << QLatin1String("urn:xmpp:http:upload"));

	if (!isUploadService) {
		foreach(const DiscoItem::Identity& i, jt->item().identities()) {
			if (i.category == QLatin1String("store") && i.type == QLatin1String("file")) {
				isUploadService = true;
				break;
			}
		}
	}

	// Debug: log what we found on each sub-component
	{
		QString svc = jt->jid().full();
		QStringList feat_list;
		// Report a few interesting features only
		foreach (const QString& f, feats.list()) {
			if (f.contains("upload") || f.contains("file") ||
			    f.contains("mam") || f.contains("carbons"))
				feat_list << f;
		}
		QStringList idents;
		foreach (const DiscoItem::Identity& i, jt->item().identities())
			idents << (i.category + "/" + i.type);
		qDebug() << "[ServerInfo] disco#info" << svc
		         << "identities=" << idents.join(",")
		         << "relevant_features=" << feat_list.join(",")
		         << "isUploadService=" << isUploadService;
	}

	if (isUploadService && (!hasHttpUpload_ || httpUploadService_.isEmpty())) {
		hasHttpUpload_ = true;
		httpUploadService_ = jt->jid().full();
		qDebug() << "[ServerInfo] XEP-0363 HTTP Upload: service =" << httpUploadService_;
		emit featuresChanged();
	}
}
