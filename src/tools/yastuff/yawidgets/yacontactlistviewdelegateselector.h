/*
 * yacontactlistviewdelegateselector.h
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

#ifndef YACONTACTLISTVIEWDELEGATESELECTOR_H
#define YACONTACTLISTVIEWDELEGATESELECTOR_H

#include <QObject>

class QTreeView;
class QModelIndex;
class QAbstractItemDelegate;

class YaContactListDelegateSelector : public QObject
{
	Q_OBJECT
public:
	YaContactListDelegateSelector(QTreeView* parent);
	~YaContactListDelegateSelector();

	enum AvatarMode {
		AvatarMode_Disable = 0,
		AvatarMode_Auto = 1,
		AvatarMode_Big = 2,
		AvatarMode_Small = 3
	};

	AvatarMode avatarMode() const;
	void setAvatarMode(AvatarMode avatarMode);

	QAbstractItemDelegate* delegateFor(const QTreeView* treeView);
	static int indexCombinedHeight(const QTreeView* treeView, const QModelIndex& index, QAbstractItemDelegate* delegate);

signals:
	void invalidate();

private slots:
	void optionChanged(const QString& option);

private:
	AvatarMode avatarMode_;
	QAbstractItemDelegate* slimDelegate_;
	QAbstractItemDelegate* normalDelegate_;
	QAbstractItemDelegate* largeDelegate_;
};

#endif
