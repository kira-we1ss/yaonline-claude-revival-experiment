/*
 * yagroupchatcontactlistmodel.cpp
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

#include "yagroupchatcontactlistmodel.h"

#include "groupchatdlg.h"
#include "psiaccount.h"
#include "avatars.h"

YaGroupchatContactListModel::YaGroupchatContactListModel(GCMainDlg* groupChat)
	: QStandardItemModel(groupChat)
	, groupChat_(groupChat)
	, id_(0)
{
	connect(groupChat_, SIGNAL(configureEnabledChanged()), SIGNAL(configureEnabledChanged()));

	moderatorGroup_ = new QStandardItem(tr("Moderators"));
	moderatorGroup_->setData(QVariant(Type_Group), TypeRole);
	moderatorGroup_->setData(QVariant(true), ContactListModel::ExpandedRole);
	invisibleRootItem()->appendRow(moderatorGroup_);
	groups_ << moderatorGroup_;

	participantGroup_ = new QStandardItem(tr("Participants"));
	participantGroup_->setData(QVariant(Type_Group), TypeRole);
	participantGroup_->setData(QVariant(true), ContactListModel::ExpandedRole);
	invisibleRootItem()->appendRow(participantGroup_);
	groups_ << participantGroup_;

	visitorGroup_ = new QStandardItem(tr("Visitors"));
	visitorGroup_->setData(QVariant(Type_Group), TypeRole);
	visitorGroup_->setData(QVariant(true), ContactListModel::ExpandedRole);
	invisibleRootItem()->appendRow(visitorGroup_);
	groups_ << visitorGroup_;

	absentGroup_ = new QStandardItem(tr("Absent"));
	absentGroup_->setData(QVariant(Type_Group), TypeRole);
	absentGroup_->setData(QVariant(true), ContactListModel::ExpandedRole);
	absentGroup_->setData(QVariant(false), VisibleRole);
	invisibleRootItem()->appendRow(absentGroup_);
}

YaGroupchatContactListModel::~YaGroupchatContactListModel()
{
}

XMPP::Status YaGroupchatContactListModel::status(const QStandardItem* item)
{
	Q_ASSERT(item);
	if (item) {
		return item->data(StatusRole).value<XMPP::Status>();
	}
	return XMPP::Status();
}

QStandardItem* YaGroupchatContactListModel::findEntry(const QString& nick) const
{
	if (!items_.contains(nick))
		return 0;
	return items_[nick];
}

void YaGroupchatContactListModel::updateEntry(const QString& nick, const XMPP::Status& status, QString newNick)
{
	QStandardItem* contact = findEntry(nick);
	newNick = !newNick.isEmpty() ? newNick : nick;
	int id = -1;
	if (contact) {
		id = contact->data(IdRole).toInt();

		if (this->status(contact).mucItem().role() != status.mucItem().role() ||
		    nick != newNick)
		{
			contact->parent()->removeRow(contact->row());
			items_[nick] = 0;
			contact = 0;
		}
	}
	else {
		if (newNick == groupChat_->nick())
			id = -1;
		else
			id = ++id_;
	}

	if (!contact) {
		contact = new QStandardItem(newNick);
		contact->setData(QVariant(Type_Contact), TypeRole);
		contact->setData(QVariant(id), IdRole);
		contact->setData(QVariant(true), VisibleRole);
		items_[newNick] = contact;
		contact->setData(QVariant(newNick), NickRole);
		parentFor(status.mucItem().role())->appendRow(contact);
	}

	QVariant statusVariant;
	statusVariant.setValue(status);
	contact->setData(statusVariant, StatusRole);
	updateGroupVisibility();
}

void YaGroupchatContactListModel::removeEntry(const QString& nick)
{
	QStandardItem* contact = findEntry(nick);
	if (contact) {
		QVariant statusVariant;
		statusVariant.setValue(Status());
		contact->setData(statusVariant, StatusRole);

		QList<QStandardItem*> items = contact->parent()->takeRow(contact->row());
		foreach(QStandardItem* i, items) {
			absentGroup_->appendRow(i);
		}
		updateGroupVisibility();
	}
}

void YaGroupchatContactListModel::updateGroupVisibility()
{
	foreach(QStandardItem* g, groups_) {
		bool visible = g->data(VisibleRole).toBool();
		bool shouldBeVisible = g->rowCount() > 0;
		if (visible != shouldBeVisible) {
			g->setData(QVariant(shouldBeVisible), VisibleRole);
		}
	}
}

QStandardItem* YaGroupchatContactListModel::parentFor(MUCItem::Role role) const
{
	QStandardItem* result = visitorGroup_;
	if (role == MUCItem::Moderator)
		result = moderatorGroup_;
	else if (role == MUCItem::Participant)
		result = participantGroup_;
	else if (role == MUCItem::UnknownRole)
		result = absentGroup_;

	return result;
}

QVariant YaGroupchatContactListModel::data(const QModelIndex& index, int role) const
{
	QStandardItem* item = itemFromIndex(index);
	if (!item) {
		return QVariant();
	}
	GC_ItemType gcType = static_cast<GC_ItemType>(item->data(TypeRole).toInt());
	if (role == ContactListModel::TypeRole) {
		if (gcType == Type_Contact)
			return ContactListModel::ContactType;
		else
			return ContactListModel::GroupType;
	}

	if (gcType == Type_Contact) {
		if (role == ContactListModel::JidRole) {
			return QVariant(status(item).mucItem().jid().full());
		}
		else if (role == ContactListModel::PictureRole) {
			XMPP::Status status = this->status(item);
			if (!status.mucItem().jid().isEmpty()) {
				QIcon icon = groupChat()->account()->avatarFactory()->getAvatar(status.mucItem().jid().bare());
				return QVariant(icon);
			}
			return QVariant(QIcon());
		}
		else if (role == ContactListModel::StatusTextRole) {
			return QVariant(status(item).status().simplified());
		}
		else if (role == ContactListModel::StatusTypeRole) {
			return QVariant(status(item).type());
		}
	}
	else {
		if (role == ContactListModel::ExpandedRole) {
			return item->data(ContactListModel::ExpandedRole).toBool();
		}
		else if (role == ContactListModel::TotalItemsRole) {
			return QVariant(item->rowCount());
		}
	}

	return QStandardItemModel::data(index, role);
}

bool YaGroupchatContactListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (role == ContactListModel::ActivateRole) {
		ContactListModel::Type type = static_cast<ContactListModel::Type>(index.data(ContactListModel::TypeRole).toInt());
		if (type == ContactListModel::GroupType) {
			setData(index, QVariant(!index.data(ContactListModel::ExpandedRole).toBool()), ContactListModel::ExpandedRole);
			return true;
		}
		else if (type == ContactListModel::ContactType) {
			emit insertNick(index.data(NickRole).toString());
		}
	}
	return QStandardItemModel::setData(index, value, role);
}

QStringList YaGroupchatContactListModel::nickList() const
{
	return items_.keys();
}

GCMainDlg* YaGroupchatContactListModel::groupChat() const
{
	return groupChat_;
}
