/*
 * srvresolver.cpp - class to simplify SRV lookups
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "srvresolver.h"

#include <QByteArray>
#include <QTimer>
#include <QDnsLookup>
#include <QHostAddress>
#include <algorithm>
#include "safedelete.h"
#include "psilogger.h"

#ifndef NO_NDNS
#include "ndns.h"
#endif

// CS_NAMESPACE_BEGIN

static bool serverLessThan(const SrvServer &s1, const SrvServer &s2)
{
	int a = s1.priority;
	int b = s2.priority;
	int j = s1.weight;
	int k = s2.weight;
	return a < b || (a == b && j < k);
}

static void sortSRVList(QList<SrvServer> &list)
{
	std::stable_sort(list.begin(), list.end(), serverLessThan);
}

class SrvResolver::Private
{
public:
	Private() {}

	QDnsLookup *qdns;
#ifndef NO_NDNS
	NDns ndns;
#endif

	bool failed;
	QHostAddress resultAddress;
	quint16 resultPort;

	bool srvonly;
	QString srv;
	QList<SrvServer> servers;
	bool aaaa;

	QTimer t;
	SafeDelete sd;
};

SrvResolver::SrvResolver(QObject *parent)
:QObject(parent)
{
	d = new Private;
	d->qdns = 0;

#ifndef NO_NDNS
	connect(&d->ndns, SIGNAL(resultsReady()), SLOT(ndns_done()));
#endif
	connect(&d->t, SIGNAL(timeout()), SLOT(t_timeout()));
	stop();
}

SrvResolver::~SrvResolver()
{
	stop();
	delete d;
}

void SrvResolver::resolve(const QString &server, const QString &type, const QString &proto, bool srvOnly)
{
	PsiLogger::instance()->log(QString("SrvResolver::resolve(%1, %2, %3, %4)").arg(server).arg(type).arg(proto).arg(srvOnly));
	stop();

	d->failed = false;
	d->srvonly = srvOnly;
	d->srv = QString("_") + type + "._" + proto + '.' + server;
	d->t.setSingleShot(true);
	d->t.start(15000);
	d->qdns = new QDnsLookup(QDnsLookup::SRV, d->srv, this);
	connect(d->qdns, SIGNAL(finished()), SLOT(qdns_done()));
	d->qdns->lookup();
}

void SrvResolver::resolve(const QString &server, const QString &type, const QString &proto)
{
	resolve(server, type, proto, false);
}

void SrvResolver::resolveSrvOnly(const QString &server, const QString &type, const QString &proto)
{
	resolve(server, type, proto, true);
}

void SrvResolver::next()
{
	if(d->servers.isEmpty())
		return;

	tryNext();
}

void SrvResolver::stop()
{
	if(d->t.isActive())
		d->t.stop();
	if(d->qdns) {
		d->qdns->disconnect(this);
		d->sd.deleteLater(d->qdns);
		d->qdns = 0;
	}
#ifndef NO_NDNS
	if(d->ndns.isBusy())
		d->ndns.stop();
#endif
	d->resultAddress = QHostAddress();
	d->resultPort = 0;
	d->servers.clear();
	d->srv = "";
	d->failed = true;
}

bool SrvResolver::isBusy() const
{
#ifndef NO_NDNS
	if(d->qdns || d->ndns.isBusy())
#else
	if(d->qdns)
#endif
		return true;
	else
		return false;
}

QList<SrvServer> SrvResolver::servers() const
{
	return d->servers;
}

bool SrvResolver::failed() const
{
	return d->failed;
}

QHostAddress SrvResolver::resultAddress() const
{
	return d->resultAddress;
}

quint16 SrvResolver::resultPort() const
{
	return d->resultPort;
}

void SrvResolver::tryNext()
{
#ifndef NO_NDNS
	PsiLogger::instance()->log(QString("SrvResolver(%1)::tryNext() d->ndns.resolve(%2)").arg(d->srv).arg(d->servers.first().name));
	d->ndns.resolve(d->servers.first().name);
#else
	QDnsLookup::Type qtype = d->aaaa ? QDnsLookup::AAAA : QDnsLookup::A;
	d->qdns = new QDnsLookup(qtype, d->servers.first().name, this);
	connect(d->qdns, SIGNAL(finished()), SLOT(ndns_done()));
	d->qdns->lookup();
#endif
}

void SrvResolver::qdns_done()
{
	if(!d->qdns)
		return;

	if(d->qdns->error() != QDnsLookup::NoError) {
		PsiLogger::instance()->log(QString("SrvResolver(%1)::qdns_done() error=%2").arg(d->srv).arg(d->qdns->errorString()));
	}

	d->t.stop();

	SafeDeleteLock s(&d->sd);

	// grab the server list and destroy the qdns object
	QList<SrvServer> list;
	for(const QDnsServiceRecord &rec : d->qdns->serviceRecords()) {
		SrvServer srv;
		srv.name     = rec.target();
		srv.port     = rec.port();
		srv.priority = rec.priority();
		srv.weight   = rec.weight();
		list.append(srv);
	}
	d->qdns->disconnect(this);
	d->sd.deleteLater(d->qdns);
	d->qdns = 0;

	if(list.isEmpty()) {
		stop();
		resultsReady();
		return;
	}
	sortSRVList(list);
	d->servers = list;

	if(d->srvonly)
		resultsReady();
	else {
		d->aaaa = true;
		tryNext();
	}
}

void SrvResolver::ndns_done()
{
#ifndef NO_NDNS
	SafeDeleteLock s(&d->sd);

	QHostAddress r = d->ndns.result();
	int port = d->servers.first().port;
	d->servers.removeFirst();

	PsiLogger::instance()->log(QString("SrvResolver(%1)::ndns_done() r.isNull = %2, r = %3, port = %4").arg(d->srv).arg(r.isNull()).arg(r.toString()).arg(port));

	if(!r.isNull()) {
		d->resultAddress = r;
		d->resultPort = port;
		resultsReady();
	}
	else {
		if(d->servers.isEmpty()) {
			stop();
			resultsReady();
			return;
		}
		tryNext();
	}
#else
	if(!d->qdns)
		return;

	SafeDeleteLock s(&d->sd);

	// grab the address list and destroy the qdns object
	QList<QHostAddress> list;
	for(const QDnsHostAddressRecord &rec : d->qdns->hostAddressRecords())
		list.append(rec.value());
	d->qdns->disconnect(this);
	d->sd.deleteLater(d->qdns);
	d->qdns = 0;

	if(!list.isEmpty()) {
		int port = d->servers.first().port;
		d->servers.removeFirst();
		d->aaaa = true;

		d->resultAddress = list.first();
		d->resultPort = port;
		resultsReady();
	}
	else {
		if(!d->aaaa)
			d->servers.removeFirst();
		d->aaaa = !d->aaaa;

		if(d->servers.isEmpty()) {
			stop();
			resultsReady();
			return;
		}
		tryNext();
	}
#endif
}

void SrvResolver::t_timeout()
{
	SafeDeleteLock s(&d->sd);

	stop();
	resultsReady();
}

// CS_NAMESPACE_END
