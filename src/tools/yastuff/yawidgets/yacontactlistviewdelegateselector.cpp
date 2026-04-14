/*
 * yacontactlistviewdelegateselector.cpp
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

#include "yacontactlistviewdelegateselector.h"

#include <QTreeView>

#include "psioptions.h"
#include "yacontactlistviewdelegate.h"
#include "yacontactlistviewslimdelegate.h"
#include "yacontactlistviewlargedelegate.h"
#include "yacontactlistview.h"

static const QString avatarStyleOptionPath = "options.ya.main-window.contact-list.avatar-style";

YaContactListDelegateSelector::YaContactListDelegateSelector(QTreeView* parent)
	: QObject(parent)
{
	YaContactListView* lv = dynamic_cast<YaContactListView*>(parent);
	slimDelegate_   = new YaContactListViewSlimDelegate(lv);
	normalDelegate_ = new YaContactListViewDelegate(lv);
	largeDelegate_  = new YaContactListViewLargeDelegate(lv);

	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
	optionChanged(avatarStyleOptionPath);
}

YaContactListDelegateSelector::~YaContactListDelegateSelector()
{
	delete slimDelegate_;
	delete normalDelegate_;
	delete largeDelegate_;
}

void YaContactListDelegateSelector::optionChanged(const QString& option)
{
	if (option == avatarStyleOptionPath) {
		setAvatarMode(static_cast<YaContactListDelegateSelector::AvatarMode>(PsiOptions::instance()->getOption(avatarStyleOptionPath).toInt()));
	}
}

YaContactListDelegateSelector::AvatarMode YaContactListDelegateSelector::avatarMode() const
{
	return avatarMode_;
}

void YaContactListDelegateSelector::setAvatarMode(YaContactListDelegateSelector::AvatarMode avatarMode)
{
	avatarMode_ = avatarMode;
	emit invalidate();
}

QAbstractItemDelegate* YaContactListDelegateSelector::delegateFor(const QTreeView* treeView)
{
	switch (avatarMode()) {
	case AvatarMode_Disable:
		return slimDelegate_;
	case AvatarMode_Big:
		return largeDelegate_;
	case AvatarMode_Small:
		return normalDelegate_;
	case AvatarMode_Auto:
	default:
		if (indexCombinedHeight(treeView, QModelIndex(), largeDelegate_) <= treeView->viewport()->height())
			return largeDelegate_;
		else
			return normalDelegate_;
	}
	return 0;
}

int YaContactListDelegateSelector::indexCombinedHeight(const QTreeView* treeView, const QModelIndex& parent, QAbstractItemDelegate* delegate)
{
	if (!delegate || !treeView->model())
		return 0;
	int result = delegate->sizeHint(QStyleOptionViewItem(), parent).height();
	for (int row = 0; row < treeView->model()->rowCount(parent); ++row) {
		QModelIndex index = treeView->model()->index(row, 0, parent);
		if (treeView->isExpanded(index))
			result += indexCombinedHeight(treeView, index, delegate);
		else
			result += delegate->sizeHint(QStyleOptionViewItem(), index).height();
	}
	return result;
}
