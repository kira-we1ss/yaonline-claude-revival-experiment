/*
 * yagroupchatcontactlistview.cpp
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

#include "yagroupchatcontactlistview.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QPalette>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

#include "yacontactlistviewdelegateselector.h"
#include "yaofficebackgroundhelper.h"
#include "smoothscrollbar.h"
#include "contactlistmodel.h"
#include "yagroupchatcontactlistmenu.h"
#include "yacontactlistmodelselection.h"
#include "psiaccount.h"
#include "yagroupchatcontactlistmodel.h"

YaGroupchatContactListView::YaGroupchatContactListView(QWidget* parent)
	: ContactListView(parent)
{
	setAcceptDrops(true);

	setEditTriggers(QAbstractItemView::NoEditTriggers);

	QAbstractItemDelegate* delegate = itemDelegate();
	delete delegate;

	delegateSelector_ = new YaContactListDelegateSelector(this);
	connect(delegateSelector_, SIGNAL(invalidate()), SLOT(invalidateDelegate()));
	invalidateDelegate();

	SmoothScrollBar::install(this);

	setMouseTracking(true);
	connect(this, SIGNAL(clicked(const QModelIndex&)), SLOT(clicked(const QModelIndex&)));

	YaOfficeBackgroundHelper::instance()->registerWidget(this);

	// Qt5/macOS: force white background on viewport (empty space below participants)
	viewport()->setAutoFillBackground(true);
	{
		QPalette vp = viewport()->palette();
		vp.setColor(QPalette::Base,   Qt::white);
		vp.setColor(QPalette::Window, Qt::white);
		viewport()->setPalette(vp);
	}
}

void YaGroupchatContactListView::setAccount(PsiAccount* account)
{
	account_ = account;
}

void YaGroupchatContactListView::setGroupchat(const QString& groupchat)
{
	groupchat_ = groupchat;
}

void YaGroupchatContactListView::invalidateDelegate()
{
	QAbstractItemDelegate* newDelegate = delegateSelector_->delegateFor(this);
	if (itemDelegate() == newDelegate)
		return;

	setItemDelegate(newDelegate);
	doItemsLayout();
}

void YaGroupchatContactListView::itemActivated(const QModelIndex& index)
{
	ContactListView::itemActivated(index);
	if (index.data(ContactListModel::TypeRole).toInt() == ContactListModel::GroupType) {
		setExpanded(index, index.data(ContactListModel::ExpandedRole).toBool());
	}
}

void YaGroupchatContactListView::clicked(const QModelIndex& index)
{
	if (index.data(ContactListModel::TypeRole).toInt() == ContactListModel::GroupType) {
		itemActivated(index);
	}
}

void YaGroupchatContactListView::mouseDoubleClickEvent(QMouseEvent* e)
{
	QModelIndex index = indexAt(e->pos());
	if (index.data(ContactListModel::TypeRole).toInt() == ContactListModel::GroupType) {
		return;
	}
	ContactListView::mouseDoubleClickEvent(e);
}

ContactListItemMenu* YaGroupchatContactListView::createContextMenuFor(const QModelIndexList& indexes) const
{
	QModelIndex index = indexes.first();
	if (index.data(ContactListModel::TypeRole).toInt() == ContactListModel::GroupType)
		return 0;

	return new YaGroupchatContactListMenu(index);
}

void YaGroupchatContactListView::itemExpanded(const QModelIndex& index)
{
	ContactListView::itemExpanded(index);
	if (index.data(ContactListModel::TypeRole).toInt() == ContactListModel::GroupType) {
		model()->blockSignals(true);
		if (!index.data(ContactListModel::ExpandedRole).toBool()) {
			itemActivated(index);
		}
		model()->blockSignals(false);
	}
}

void YaGroupchatContactListView::itemCollapsed(const QModelIndex& index)
{
	ContactListView::itemCollapsed(index);
	if (index.data(ContactListModel::TypeRole).toInt() == ContactListModel::GroupType) {
		model()->blockSignals(true);
		if (index.data(ContactListModel::ExpandedRole).toBool()) {
			itemActivated(index);
		}
		model()->blockSignals(false);
	}
}

void YaGroupchatContactListView::dragEnterEvent(QDragEnterEvent* event)
{
	event->accept();
}

void YaGroupchatContactListView::dragLeaveEvent(QDragLeaveEvent* event)
{
	event->accept();
}

void YaGroupchatContactListView::dragMoveEvent(QDragMoveEvent* event)
{
	event->accept();
}

void YaGroupchatContactListView::dropEvent(QDropEvent* event)
{
	YaContactListModelSelection selection(event->mimeData());
	foreach(const YaContactListModelSelection::Contact c, selection.contacts()) {
		account_->actionInvite(c.jid, groupchat_);
		qWarning("Sent invite (%s) to %s", qPrintable(groupchat_), qPrintable(c.jid));
	}
}

void YaGroupchatContactListView::setModel(QAbstractItemModel* model)
{
	ContactListView::setModel(model);

	QModelIndex parent = QModelIndex();
	for (int row = 0; row <= model->rowCount(parent); ++row) {
		QModelIndex index = model->index(row, 0, parent);
		setRowHidden(row, parent, !index.data(YaGroupchatContactListModel::VisibleRole).toBool());
	}
}

void YaGroupchatContactListView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
	QModelIndex parent = topLeft.parent();
	for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
		QModelIndex index = model()->index(row, 0, parent);
		setRowHidden(row, parent, !index.data(YaGroupchatContactListModel::VisibleRole).toBool());
	}
}
