/*
 * xmpp_csi.h - XEP-0352 Client State Indication
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

#ifndef XMPP_CSI_H
#define XMPP_CSI_H

// XEP-0352 Client State Indication
// Send <active/> or <inactive/> to server based on app focus state.
// Namespace: urn:xmpp:csi:0
// Server must advertise urn:xmpp:csi:0 in stream features (or disco).

#include <QObject>

namespace XMPP {
	class Client;

	class ClientStateIndication : public QObject
	{
		Q_OBJECT
	public:
		static const char* NS;

		explicit ClientStateIndication(Client* client, QObject* parent = nullptr);

		// Call when app window gains focus / becomes active
		void setActive();
		// Call when app window loses focus / is minimized
		void setInactive();

		bool isServerSupported() const { return serverSupported_; }
		void setServerSupported(bool s) { serverSupported_ = s; }

	private:
		void sendElement(const QString& tagName);

		Client*  client_;
		bool     serverSupported_;
		bool     currentlyActive_;
	};

} // namespace XMPP

#endif // XMPP_CSI_H
