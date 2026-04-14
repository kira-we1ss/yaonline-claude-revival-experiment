/*
 * yapddmanager.cpp
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

#include "yapddmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDomDocument>
#include <QTimer>

#include "yacommon.h"
#include "xmpp_xmlcommon.h"
#include "psilogger.h"

const static int domainDataExpirationInterval = 10; // in minutes
const static int deadRequestsTimerInterval = 5; // in seconds
const static int deadRequestsTTL = 15; // in seconds

YaPddManager::YaPddManager(PsiCon* controller, QObject* parent)
	: BaseHttpHelper<QObject>(controller, parent)
{
	deadRequestsTimer_ = new QTimer(this);
	deadRequestsTimer_->setSingleShot(true);
	deadRequestsTimer_->setInterval(deadRequestsTimerInterval * 1000);
	connect(deadRequestsTimer_, SIGNAL(timeout()), SLOT(checkDeadRequests()));
}

YaPddManager::~YaPddManager()
{
}

bool YaPddManager::knownDomain(const QString& domain) const
{
	if (domainData_.contains(domain)) {
		int secs = domainData_[domain].lastCheckedAt.secsTo(QDateTime::currentDateTime());
		return qAbs(secs) < (domainDataExpirationInterval * 60);
	}
	return false;
}

YaPddManager::DomainData YaPddManager::domainData(const QString& domain) const
{
	if (!knownDomain(domain))
		return DomainData();

	return domainData_[domain];
}

void YaPddManager::requestDomainData(const QString& domain)
{
	PsiLogger::instance()->log(QString("YaPddManager::requestDomainData(%1)")
	                           .arg(domain));

	Q_ASSERT(!Ya::isYaRuDomain(domain));
	if (knownDomain(domain)) {
		Q_ASSERT(false);
		return;
	}

	QMutableListIterator<Request> it(requestQueue_);
	while (it.hasNext()) {
		Request request = it.next();
		if (request.domain == domain)
			return;
	}

	Request request;
	request.domain = domain;
	request.lastCheckedAt = QDateTime::currentDateTime();
	request.reply = network()->get(
	                    HttpHelper::getRequest(
	                        QString("http://mobile.online.yandex.net/yaonline/xmppresolve?server=%1")
	                        .arg(request.domain)));
	requestQueue_ << request;
	startCheckDeadRequests();
}

void YaPddManager::startCheckDeadRequests()
{
	if (!requestQueue_.isEmpty()) {
		PsiLogger::instance()->log(QString("YaPddManager::startCheckDeadRequests (%1 requests in queue)")
		                           .arg(requestQueue_.size()));
		deadRequestsTimer_->start();
	}
}

void YaPddManager::checkDeadRequests()
{
	PsiLogger::instance()->log(QString(QString("YaPddManager::checkDeadRequests (%1 requests in queue)")
	                                   .arg(requestQueue_.size())));
	QStringList domainsToCheck;

	QMutableListIterator<Request> it(requestQueue_);
	while (it.hasNext()) {
		Request request = it.next();
		int secs = request.lastCheckedAt.secsTo(QDateTime::currentDateTime());
		bool ttlOk = qAbs(secs) < deadRequestsTTL;
		PsiLogger::instance()->log(QString("YaPddManager::checkDeadRequests: %1 (%2 secs) to_be_discarded = %3")
		                           .arg(request.domain)
		                           .arg(qAbs(secs))
		                           .arg(!ttlOk));
		if (ttlOk)
			continue;

		it.remove();
		domainsToCheck << request.domain;
	}

	foreach(QString d, domainsToCheck) {
		requestDomainData(d);
	}

	startCheckDeadRequests();
}

struct YaPddServerData {
	QString host;
	int port;
	int priority;

	bool operator<(const YaPddServerData& other) const {
		return priority < other.priority;
	}
};

void YaPddManager::replyFinished(QNetworkReply* reply)
{
	reply->deleteLater();

	QString requestDomain;
	QMutableListIterator<Request> it(requestQueue_);
	while (it.hasNext()) {
		Request request = it.next();
		if (request.reply != reply)
			continue;

		PsiLogger::instance()->log(QString("YaPddManager::replyFinished(%1) error %2")
		                           .arg(request.domain)
		                           .arg(reply->error()));
		it.remove();

		requestDomain = request.domain;

		if (reply->error() != QNetworkReply::NoError) {
			qWarning("YaPddManager::replyFinished(): error");
			break;
		}

		QByteArray data = reply->readAll();
		QString text = QString::fromUtf8(data);

		QDomDocument doc;
		if (doc.setContent(data)) {
			QDomElement root = doc.documentElement();
			if (root.tagName() == "xmppinfo") {
				DomainData data;
				data.lastCheckedAt = QDateTime::currentDateTime();

				QList<YaPddServerData> serverData;
				for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
					QDomElement e = n.toElement();
					if (e.isNull() || e.tagName() != "server")
						continue;

					YaPddServerData d;
					d.host = e.attribute("host");
					d.port = e.attribute("port").toInt();
					d.priority = e.attribute("priority").toInt();
					serverData << d;
				}

				if (!serverData.isEmpty()) {
					qSort(serverData.begin(), serverData.end());

					data.connectToHost = serverData.first().host;
					data.connectToPort = serverData.first().port;
					data.state = State_PddThirdParty;
				}
				else {
					data.state = State_Unknown;
				}

				domainData_[request.domain] = data;
				emit domainDataChanged();
			}
		}
		break;
	}

	if (!requestDomain.isEmpty()) {
		emit domainDataChanged(requestDomain);
	}
	startCheckDeadRequests();
}
