/*
 * yagroupchatcontactlistmodel.h
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

#ifndef YAGROUPCHATCONTACTLISTMODEL_H
#define YAGROUPCHATCONTACTLISTMODEL_H

#include <QStandardItemModel>
#include <QPointer>
#include <QHash>

class GCMainDlg;

#include "xmpp_muc.h"
#include "xmpp_status.h"
#include "contactlistmodel.h"

class YaGroupchatContactListModel : public QStandardItemModel
{
	Q_OBJECT
public:
	YaGroupchatContactListModel(GCMainDlg* groupChat);
	~YaGroupchatContactListModel();

	enum {
		NickRole   = ContactListModel::LastRole + 1,
		StatusRole = ContactListModel::LastRole + 2,
		TypeRole   = ContactListModel::LastRole + 3,
		IdRole     = ContactListModel::LastRole + 4,
		VisibleRole= ContactListModel::LastRole + 5,
		LastRole = ContactListModel::LastRole + 5
	};

	enum GC_ItemType {
		Type_Group,
		Type_Contact
	};

	GCMainDlg* groupChat() const;
	QStringList nickList() const;

	// reimplemented
	QVariant data(const QModelIndex& index, int role) const;
	bool setData(const QModelIndex&, const QVariant&, int role);

	QStandardItem* findEntry(const QString& nick) const;
	void updateEntry(const QString& nick, const XMPP::Status& status, QString newNick = QString());
	void removeEntry(const QString& nick);

	static XMPP::Status status(const QStandardItem* item);

signals:
	void insertNick(const QString& nick);
	void configureEnabledChanged();

protected:
	QStandardItem* parentFor(XMPP::MUCItem::Role role) const;
	void updateGroupVisibility();

private:
	QPointer<GCMainDlg> groupChat_;
	QHash<QString, QStandardItem*> items_;

	QStandardItem* moderatorGroup_;
	QStandardItem* participantGroup_;
	QStandardItem* visitorGroup_;
	QStandardItem* absentGroup_;
	QList<QStandardItem*> groups_;
	int id_;
};

#endif
