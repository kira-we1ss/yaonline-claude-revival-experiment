/*
 * yagroupchatroomlist.h
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

#ifndef YAGROUPCHATROOMLIST_H
#define YAGROUPCHATROOMLIST_H

#include <QTreeWidget>
#include <QPointer>

class PsiCon;
class PsiAccount;

#include "xmpp_jid.h"

namespace XMPP {
	class JT_DiscoItems;
};

class YaGroupChatRoomList : public QTreeWidget
{
	Q_OBJECT
public:
	YaGroupChatRoomList(QWidget* parent);
	~YaGroupChatRoomList();

	void addRoom(const XMPP::Jid& roomJid);
	bool sortByOccupants() const;
	void setSortByOccupants(bool sortByOccupants);

	QString currentJid() const;

signals:
	void joinRoom(const XMPP::Jid& roomJid);

public slots:
	void setAccount(PsiAccount* account);
	void setFilterText(const QString& filterText);
	void browseServer(const QString& serverJid);

private slots:
	void doubleClicked(QTreeWidgetItem* item, int column);
	void discoInfoFinished();
	void browseServerFinished();

protected:
	// reimplemented
	void resizeEvent(QResizeEvent* e);

	void updateItemVisibility(QTreeWidgetItem* item);

private:
	bool sortByOccupants_;
	QString filterText_;
	QRegExp filterRegExp_;
	QPointer<PsiAccount> account_;
	QPointer<XMPP::JT_DiscoItems> browseServerTask_;
};

#endif
