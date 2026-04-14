/*
 * yaaccountselector.cpp
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

#include "yaaccountselector.h"

#include "psicon.h"
#include "psicontactlist.h"
#include "psiaccount.h"

YaAccountSelector::YaAccountSelector(QWidget* parent)
	: QComboBox(parent)
{
}

YaAccountSelector::~YaAccountSelector()
{
}

void YaAccountSelector::setController(PsiCon* controller)
{
	controller_ = controller;
	connect(controller_->contactList(), SIGNAL(accountCountChanged()), this, SLOT(accountCountChanged()));
	accountCountChanged();
}

QString YaAccountSelector::currentAccountId() const
{
	if (currentIndex() == -1)
		return QString();

	return itemData(currentIndex()).toString();
}

void YaAccountSelector::accountCountChanged()
{
	if (!controller_->contactList()->accountsLoaded())
		return;

	setUpdatesEnabled(false);

	QString currentAccountId = this->currentAccountId();
	clear();
	foreach(const PsiAccount* account, controller_->contactList()->enabledAccounts()) {
		addItem(account->name(), account->id());
		if (account->id() == currentAccountId) {
			setCurrentIndex(count()-1);
		}
	}

	setUpdatesEnabled(true);
}
