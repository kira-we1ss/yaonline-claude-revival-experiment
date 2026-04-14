/*
 * yagroupchatcontactlistmenu.h
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

#ifndef YAGROUPCHATCONTACTLISTMENU_H
#define YAGROUPCHATCONTACTLISTMENU_H

#include "contactlistitemmenu.h"
#include <QPointer>

class QModelIndex;
class YaGroupchatContactListModel;

#include "xmpp_status.h"

class YaGroupchatContactListMenu : public ContactListItemMenu
{
	Q_OBJECT
public:
	YaGroupchatContactListMenu(const QModelIndex& index);
	~YaGroupchatContactListMenu();

private slots:
	void doInsertNick();
	void doOpenChat();
	void doKick();
	void doBan();
	void doVCard();
	void doChangeRoleModerator();
	void doChangeRoleParticipant();
	void doChangeRoleVisitor();

	void update();

private:
	QPointer<YaGroupchatContactListModel> model_;
	QString nick_;
	XMPP::Status status_;

	QAction* insertNick_;
	QAction* openChat_;
	QAction* kick_;
	QAction* ban_;
	QAction* vCard_;
	QMenu* changeRoleMenu_;
	QAction* changeRoleModerator_;
	QAction* changeRoleParticipant_;
	QAction* changeRoleVisitor_;
};

#endif
