/*
 * serverlistquerier.cpp
 * Copyright (C) 2007  Remko Troncon
 * Qt5 port: QHttp -> QNetworkAccessManager
 */

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNodeList>
#include <QUrl>

#include "serverlistquerier.h"

#define SERVERLIST_URL "http://www.jabber.org/servers.xml"

ServerListQuerier::ServerListQuerier(QObject* parent) : QObject(parent)
{
	nam_ = new QNetworkAccessManager(this);
	connect(nam_, &QNetworkAccessManager::finished, this, &ServerListQuerier::replyFinished);
}

void ServerListQuerier::getList()
{
	nam_->get(QNetworkRequest(QUrl(SERVERLIST_URL)));
}

void ServerListQuerier::replyFinished(QNetworkReply *reply)
{
	if (reply->error() != QNetworkReply::NoError) {
		emit error(reply->errorString());
	}
	else {
		QDomDocument doc;
		if (!doc.setContent(reply->readAll())) {
			emit error(tr("Unable to parse server list"));
			reply->deleteLater();
			return;
		}

		QStringList servers;
		QDomNodeList items = doc.elementsByTagName("item");
		for (int i = 0; i < items.count(); i++) {
			QString jid = items.item(i).toElement().attribute("jid");
			if (!jid.isEmpty())
				servers.push_back(jid);
		}
		emit listReceived(servers);
	}
	reply->deleteLater();
}
