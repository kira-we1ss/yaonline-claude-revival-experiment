/*
 * yagroupchatcontactlistmenu.cpp
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

#include "yagroupchatcontactlistmenu.h"

#include "yagroupchatcontactlistmodel.h"
#include "groupchatdlg.h"
#include "yaboldmenu.h"

YaGroupchatContactListMenu::YaGroupchatContactListMenu(const QModelIndex& index)
	: ContactListItemMenu(0, 0)
{
	const YaGroupchatContactListModel* model = dynamic_cast<const YaGroupchatContactListModel*>(index.model());
	Q_ASSERT(model);
	model_ = const_cast<YaGroupchatContactListModel*>(model);
	connect(model_, SIGNAL(configureEnabledChanged()), SLOT(update()));
	connect(this, SIGNAL(aboutToShow()), SLOT(update()));

	nick_ = index.data(YaGroupchatContactListModel::NickRole).toString();
	status_ = model->status(model->itemFromIndex(index));

	insertNick_ = new QAction(tr("Insert Nick"), this);
	connect(insertNick_, SIGNAL(triggered()), SLOT(doInsertNick()));
	YaBoldMenu::ensureActionBoldText(insertNick_);

	openChat_ = new QAction(tr("Open Chat"), this);
	connect(openChat_, SIGNAL(triggered()), SLOT(doOpenChat()));

	kick_ = new QAction(tr("Kick"), this);
	connect(kick_, SIGNAL(triggered()), SLOT(doKick()));

	ban_ = new QAction(tr("Ban"), this);
	connect(ban_, SIGNAL(triggered()), SLOT(doBan()));

	vCard_ = new QAction(tr("vCard"), this);
	connect(vCard_, SIGNAL(triggered()), SLOT(doVCard()));

	changeRoleModerator_ = new QAction(tr("Moderator"), this);
	changeRoleModerator_->setCheckable(true);
	connect(changeRoleModerator_, SIGNAL(triggered()), SLOT(doChangeRoleModerator()));

	changeRoleParticipant_ = new QAction(tr("Participant"), this);
	changeRoleParticipant_->setCheckable(true);
	connect(changeRoleParticipant_, SIGNAL(triggered()), SLOT(doChangeRoleParticipant()));

	changeRoleVisitor_ = new QAction(tr("Visitor"), this);
	changeRoleVisitor_->setCheckable(true);
	connect(changeRoleVisitor_, SIGNAL(triggered()), SLOT(doChangeRoleVisitor()));

	changeRoleMenu_ = new QMenu(tr("Change Role"));

	addAction(insertNick_);
	addAction(openChat_);
	addSeparator();
	addAction(kick_);
	addAction(ban_);
	addSeparator();
	addMenu(changeRoleMenu_);
	changeRoleMenu_->addAction(changeRoleModerator_);
	changeRoleMenu_->addAction(changeRoleParticipant_);
	changeRoleMenu_->addAction(changeRoleVisitor_);
	addSeparator();
	addAction(vCard_);

	update();
}

YaGroupchatContactListMenu::~YaGroupchatContactListMenu()
{
}

void YaGroupchatContactListMenu::update()
{
	if (!model_ || !model_->groupChat())
		return;

	bool isSelf = nick_ == model_->groupChat()->nick();
	bool configureEnabled = model_->groupChat()->configureEnabled() && !isSelf;
	openChat_->setEnabled(!isSelf);
	kick_->setEnabled(configureEnabled);
	ban_->setEnabled(configureEnabled);
	changeRoleMenu_->setEnabled(configureEnabled);
	changeRoleModerator_->setEnabled(configureEnabled);
	changeRoleParticipant_->setEnabled(configureEnabled);
	changeRoleVisitor_->setEnabled(configureEnabled);

	QStandardItem* contact = model_->findEntry(nick_);
	changeRoleModerator_->setChecked(YaGroupchatContactListModel::status(contact).mucItem().role() == MUCItem::Moderator);
	changeRoleParticipant_->setChecked(YaGroupchatContactListModel::status(contact).mucItem().role() == MUCItem::Participant);
	changeRoleVisitor_->setChecked(YaGroupchatContactListModel::status(contact).mucItem().role() == MUCItem::Visitor);
}

void YaGroupchatContactListMenu::doInsertNick()
{
	if (!model_ || !model_->groupChat())
		return;
	model_->groupChat()->insertNick(nick_);
}

void YaGroupchatContactListMenu::doOpenChat()
{
	if (!model_ || !model_->groupChat())
		return;
	model_->groupChat()->openChat(nick_);
}

void YaGroupchatContactListMenu::doKick()
{
	if (!model_ || !model_->groupChat())
		return;
	model_->groupChat()->kick(nick_);
}

void YaGroupchatContactListMenu::doBan()
{
	if (!model_ || !model_->groupChat())
		return;
	model_->groupChat()->ban(nick_);
}

void YaGroupchatContactListMenu::doVCard()
{
	if (!model_ || !model_->groupChat())
		return;
	model_->groupChat()->vCard(nick_);
}

void YaGroupchatContactListMenu::doChangeRoleModerator()
{
	if (!model_ || !model_->groupChat())
		return;
	model_->groupChat()->changeRole(nick_, MUCItem::Moderator);
}

void YaGroupchatContactListMenu::doChangeRoleParticipant()
{
	if (!model_ || !model_->groupChat())
		return;
	model_->groupChat()->changeRole(nick_, MUCItem::Participant);
}

void YaGroupchatContactListMenu::doChangeRoleVisitor()
{
	if (!model_ || !model_->groupChat())
		return;
	model_->groupChat()->changeRole(nick_, MUCItem::Visitor);
}
