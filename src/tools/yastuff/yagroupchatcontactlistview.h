/*
 * yagroupchatcontactlistview.h
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

#ifndef YAGROUPCHATCONTACTLISTVIEW_H
#define YAGROUPCHATCONTACTLISTVIEW_H

// #include "yacontactlistview.h"
#include "contactlistview.h"

#include <QPointer>

class YaContactListDelegateSelector;
class PsiAccount;

class YaGroupchatContactListView : public ContactListView
{
	Q_OBJECT
public:
	YaGroupchatContactListView(QWidget* parent);

	void setAccount(PsiAccount* account);
	void setGroupchat(const QString& groupchat);

	// reimplemented
	void setModel(QAbstractItemModel* model);

private slots:
	// reimplemented
	void itemActivated(const QModelIndex& index);
	virtual void itemExpanded(const QModelIndex&);
	virtual void itemCollapsed(const QModelIndex&);
	virtual void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

	void invalidateDelegate();
	void clicked(const QModelIndex& index);

protected:
	// reimplemented
	void mouseDoubleClickEvent(QMouseEvent* e);
	ContactListItemMenu* createContextMenuFor(const QModelIndexList& indexes) const;

	// reimplemented
	virtual void dragEnterEvent(QDragEnterEvent* event);
	virtual void dragLeaveEvent(QDragLeaveEvent* event);
	virtual void dragMoveEvent(QDragMoveEvent* event);
	virtual void dropEvent(QDropEvent* event);

private:
	QPointer<PsiAccount> account_;
	QString groupchat_;
	YaContactListDelegateSelector* delegateSelector_;
};

#endif
