/*
 * groupchatcontactmenu.cpp
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

#include "groupchatcontactmenu.h"

#include <QPointer>

#include "psiaccount.h"
#include "psioptions.h"
#include "contactlistmodel.h"
#include "shortcutmanager.h"
#include "psicon.h"
#include "groupchatcontact.h"

class GroupchatContactMenu::Private : public QObject
{
	Q_OBJECT

	QPointer<GroupchatContact> contact_;
	GroupchatContactMenu* menu_;

public:
	QAction* rename_;
	QAction* remove_;

public:
	Private(GroupchatContactMenu* menu, GroupchatContact* _contact)
		: QObject(0)
		, contact_(_contact)
		, menu_(menu)
	{
		connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionChanged(const QString&)));
		connect(menu, SIGNAL(aboutToShow()), SLOT(updateActions()));

		connect(contact_, SIGNAL(updated()), SLOT(updateActions()));

		rename_ = new QAction(tr("Re&name"), this);
		rename_->setShortcuts(menu->shortcuts("contactlist.rename"));
		connect(rename_, SIGNAL(activated()), this, SLOT(rename()));

		remove_ = new QAction(tr("&Remove"), this);
		remove_->setShortcuts(ShortcutManager::instance()->shortcuts("contactlist.delete"));
		connect(remove_, SIGNAL(activated()), SLOT(removeContact()));

		updateActions();

		menu->addAction(rename_);
		menu->addAction(remove_);
	}

	~Private()
	{
	}

private slots:
	void optionChanged(const QString& option)
	{
		Q_UNUSED(option);
	}

	void updateActions()
	{
		if (!contact_)
			return;

		rename_->setEnabled(contact_->isEditable());
		remove_->setVisible(contact_->removeAvailable());
	}

	void rename()
	{
		if (contact_) {
			menu_->model()->renameSelectedItem();
		}
	}

	void removeContact()
	{
		emit menu_->removeSelection();
	}
};

GroupchatContactMenu::GroupchatContactMenu(GroupchatContact* contact, ContactListModel* model)
	: ContactListItemMenu(contact, model)
{
	d = new Private(this, contact);
}

GroupchatContactMenu::~GroupchatContactMenu()
{
	delete d;
}

// copied from PsiContactMenu::availableActions
QList<QAction*> GroupchatContactMenu::availableActions() const
{
	QList<QAction*> result;
	foreach(QAction* a, ContactListItemMenu::availableActions()) {
		if (a != d->remove_)
			result << a;
	}
	return result;
}

#include "groupchatcontactmenu.moc"
