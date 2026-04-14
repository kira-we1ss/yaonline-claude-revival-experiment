/*
 * yagroupchatroomlist.cpp
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

#include "yagroupchatroomlist.h"

#include <QHeaderView>

#include "xmpp_client.h"
#include "psiaccount.h"
#include "xmpp_discoinfotask.h"
#include "xmpp_tasks.h"
#include "yacommon.h"

YaGroupChatRoomList::YaGroupChatRoomList(QWidget* parent)
	: QTreeWidget(parent)
	, sortByOccupants_(false)
{
	QTreeWidgetItem* headerItem = new QTreeWidgetItem(0);
	headerItem->setText(0, tr( "Title" ));
	// headerItem->setText(1, tr( "Count" ));
	headerItem->setIcon(1, QIcon(":images/groupchat/occupants.png"));
	headerItem->setText(2, tr( "JID" ));
	setHeaderItem(headerItem);

	header()->setStretchLastSection(false);
	setRootIsDecorated(false);
	setSortByOccupants(true);

	connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), SLOT(doubleClicked(QTreeWidgetItem*, int)));
}

YaGroupChatRoomList::~YaGroupChatRoomList()
{
}

void YaGroupChatRoomList::resizeEvent(QResizeEvent* e)
{
	QTreeWidget::resizeEvent(e);

	QHeaderView* h = header();
	// h->resizeSection(2, h->fontMetrics().width(headerItem()->text(2)) * 2);
	// float remainingWidth = viewport()->width() - h->sectionSize(2);
	// h->resizeSection(1, int(remainingWidth * 0.3));
	// h->resizeSection(0, int(remainingWidth * 0.7));
	float remainingWidth = viewport()->width();
	h->resizeSection(0, int(remainingWidth * 0.6));
	h->resizeSection(1, int(remainingWidth * 0.1));
	h->resizeSection(2, int(remainingWidth * 0.3));
}

void YaGroupChatRoomList::setAccount(PsiAccount* account)
{
	account_ = account;
}

void YaGroupChatRoomList::addRoom(const XMPP::Jid& roomJid)
{
	if (roomJid.bare().isEmpty())
		return;

	QTreeWidgetItem* item = new QTreeWidgetItem(this);
	item->setText(0, roomJid.bare());
	item->setData(1, Qt::DisplayRole, 0);
	item->setText(2, roomJid.bare());

	bool updatesEnabled = this->updatesEnabled();
	bool sortingEnabled = this->isSortingEnabled();
	setUpdatesEnabled(false);
	setSortingEnabled(false);

	addTopLevelItem(item);
	updateItemVisibility(item);

	setSortingEnabled(sortingEnabled);
	setUpdatesEnabled(updatesEnabled);

	if (account_ && account_->isAvailable()) {
		XMPP::DiscoInfoTask* task = new XMPP::DiscoInfoTask(account_->client()->rootTask());
		connect(task, SIGNAL(finished()), SLOT(discoInfoFinished()));
		task->get(roomJid.bare());
		task->go();
	}
}

void YaGroupChatRoomList::updateItemVisibility(QTreeWidgetItem* item)
{
	bool visible = filterText_.isEmpty();
	if (!filterText_.isEmpty()) {
		visible = filterRegExp_.indexIn(item->text(0)) != -1 ||
		          filterRegExp_.indexIn(item->text(2)) != -1;
	}

	QModelIndex index = indexFromItem(item);
	setRowHidden(index.row(), index.parent(), !visible);
}

void YaGroupChatRoomList::setFilterText(const QString& filterText)
{
	if (filterText_ == filterText)
		return;

	filterText_ = filterText;
	filterRegExp_ = QRegExp(Ya::contactListFilterRegExp(filterText_), Qt::CaseInsensitive);
	bool sortingEnabled = this->isSortingEnabled();
	setUpdatesEnabled(false);
	setSortingEnabled(false);

	for (int i = 0; i < topLevelItemCount(); ++i) {
		updateItemVisibility(topLevelItem(i));
	}

	setSortingEnabled(sortingEnabled);
	setUpdatesEnabled(true);
}

void YaGroupChatRoomList::discoInfoFinished()
{
	QString title;
	int occupants = 0;
	XMPP::DiscoInfoTask* task = static_cast<XMPP::DiscoInfoTask*>(sender());
	foreach(const XMPP::DiscoItem::Identity& i, task->item().identities()) {
		if (!i.name.isEmpty()) {
			title = i.name;
			break;
		}
	}

	foreach(const XMPP::XData::Field& f, task->xdata().fields()) {
		if (f.var() == "muc#roominfo_occupants") {
			occupants = f.value().join("").toInt();
			break;
		}
	}

	for (int i = 0; i < topLevelItemCount(); ++i) {
		QTreeWidgetItem* item = topLevelItem(i);
		if (item->text(2) == task->jid().bare()) {
			if (!title.isEmpty())
				item->setText(0, title.simplified());
			item->setData(1, Qt::DisplayRole, occupants);
			break;
		}
	}
}

void YaGroupChatRoomList::doubleClicked(QTreeWidgetItem* item, int column)
{
	Q_UNUSED(column);
	emit joinRoom(item->text(2));
}

QString YaGroupChatRoomList::currentJid() const
{
	if (!currentItem())
		return QString();
	return currentItem()->text(2);
}

void YaGroupChatRoomList::browseServer(const QString& serverJid)
{
	delete browseServerTask_;
	if (!account_)
		return;

	clear();

	QTreeWidgetItem* headerItem = new QTreeWidgetItem(this);
	headerItem->setText(0, tr( "Loading..." ));

	browseServerTask_ = new JT_DiscoItems(account_->client()->rootTask());
	connect(browseServerTask_, SIGNAL(finished()), SLOT(browseServerFinished()));
	browseServerTask_->get(serverJid, QString());
	browseServerTask_->go(true);
}

void YaGroupChatRoomList::browseServerFinished()
{
	XMPP::JT_DiscoItems* task = static_cast<XMPP::JT_DiscoItems*>(sender());

	clear();

	bool sortingEnabled = this->isSortingEnabled();
	setUpdatesEnabled(false);
	setSortingEnabled(false);

	foreach(const XMPP::DiscoItem& i, task->items()) {
		addRoom(i.jid());
		// i.name()
	}

	setSortingEnabled(sortingEnabled);
	setUpdatesEnabled(true);
}

bool YaGroupChatRoomList::sortByOccupants() const
{
	return sortByOccupants_;
}

void YaGroupChatRoomList::setSortByOccupants(bool sortByOccupants)
{
	sortByOccupants_ = sortByOccupants;
	if (sortByOccupants_) {
		setSortingEnabled(true);
		sortByColumn(1, Qt::DescendingOrder);
	}
	else {
		setSortingEnabled(false);
	}
}
