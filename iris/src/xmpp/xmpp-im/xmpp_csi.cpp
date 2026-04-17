/*
 * xmpp_csi.cpp - XEP-0352 Client State Indication
 * Copyright (C) 2024  YaChat Authors
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
 */

#include "xmpp_csi.h"
#include "xmpp_client.h"

#include <QDomDocument>
#include <QString>

namespace XMPP {

const char* ClientStateIndication::NS = "urn:xmpp:csi:0";

ClientStateIndication::ClientStateIndication(Client* client, QObject* parent)
	: QObject(parent)
	, client_(client)
	, serverSupported_(false)
	, currentlyActive_(true)
{
}

void ClientStateIndication::setActive()
{
	if (!serverSupported_ || currentlyActive_)
		return;
	currentlyActive_ = true;
	sendElement("active");
}

void ClientStateIndication::setInactive()
{
	if (!serverSupported_ || !currentlyActive_)
		return;
	currentlyActive_ = false;
	sendElement("inactive");
}

void ClientStateIndication::sendElement(const QString& tagName)
{
	if (!client_)
		return;
	QDomDocument doc;
	QDomElement e = doc.createElementNS(QString::fromLatin1(NS), tagName);
	client_->send(e);
}

} // namespace XMPP
