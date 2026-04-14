/*
 * yapddmanager.h
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

#ifndef YAPDDMANAGER_H
#define YAPDDMANAGER_H

#include <QObject>

#include <QHash>
#include <QDateTime>
#include "httphelper.h"

class QTimer;

class YaPddManager : public BaseHttpHelper<QObject>
{
	Q_OBJECT
public:
	YaPddManager(PsiCon* controller, QObject* parent);
	~YaPddManager();

	enum DomainState {
		State_Unknown,
		State_PddThirdParty // PDD-enabled third-party
		// State_ThirdParty // third-party
	};

	struct DomainData {
		DomainData()
			: connectToHost("")
			, connectToPort(5222)
			, state(State_Unknown)
		{}
		QString connectToHost;
		int connectToPort;
		DomainState state;
		QDateTime lastCheckedAt;
	};

	bool knownDomain(const QString& domain) const;
	DomainData domainData(const QString& domain) const;
	void requestDomainData(const QString& domain);

signals:
	void domainDataChanged();
	void domainDataChanged(const QString& domain);

private slots:
	// reimplemented
	void replyFinished(QNetworkReply* reply);

	void startCheckDeadRequests();
	void checkDeadRequests();

private:
	struct Request {
		Request()
			: reply(0)
		{}
		QString domain;
		QNetworkReply* reply;
		QDateTime lastCheckedAt;
	};

	QHash<QString, DomainData> domainData_;
	QList<Request> requestQueue_;
	QTimer* deadRequestsTimer_;
};

#endif
