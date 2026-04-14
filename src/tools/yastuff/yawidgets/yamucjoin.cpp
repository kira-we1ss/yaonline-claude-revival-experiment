/*
 * yamucjoin.cpp
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

#include "yamucjoin.h"

#include <QMessageBox>

#include "yapushbutton.h"
#include "psicon.h"
#include "psiaccount.h"
#include "groupchatdlg.h"
#include "psicontactlist.h"

YaMucJoinDlg::YaMucJoinDlg(PsiCon* controller, PsiAccount* defaultAccount)
	: BaseMucJoinDlg()
	, controller_(controller)
	, theme_(YaWindowTheme::Unthemable)
	, nicknameAttempt_(0)
{
	ui_.setupUi(this);
	updateContentsMargins();

	ui_.cb_account->setAutoHide(true);
	ui_.cb_account->setController(controller_);
	if (defaultAccount)
		ui_.cb_account->setAccount(defaultAccount);
	controller_->dialogRegister(this);

	setMinimizeEnabled(false);
	setMaximizeEnabled(false);

	YaPushButton* okButton = new YaPushButton(this);
	okButton->init();
	okButton->setText(tr("OK"));
	okButton->setDefault(true);
	ui_.buttonBox->addButton(okButton, QDialogButtonBox::AcceptRole);
	connect(okButton, SIGNAL(clicked()), SLOT(accept()));

	YaPushButton* cancelButton = new YaPushButton(this);
	cancelButton->init();
	cancelButton->setText(trUtf8("Отмена"));
	cancelButton->setButtonStyle(YaPushButton::ButtonStyle_Destructive);
	ui_.buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);
	connect(cancelButton, SIGNAL(clicked()), SLOT(close()));

	ui_.findRoomsFilter->setEmptyText(trUtf8("Поиск по названию и адресу комнаты"));
	ui_.findRoomsFilter->setOkButtonVisible(false);
	ui_.findRoomsFilter->setCancelButtonVisible(false);
	ui_.le_room->setEmptyText(trUtf8("Например, talks"));
	ui_.le_room->setOkButtonVisible(false);
	ui_.le_room->setCancelButtonVisible(false);
	ui_.le_title->setEmptyText(trUtf8("Например, «Просто разговоры»"));
	ui_.le_title->setOkButtonVisible(false);
	ui_.le_title->setCancelButtonVisible(false);
	ui_.le_password->setOkButtonVisible(false);
	ui_.le_password->setCancelButtonVisible(false);

	connect(ui_.ck_password, SIGNAL(clicked()), SLOT(updatePasswordVisibility()));
	updatePasswordVisibility();

#ifdef YAPSI
	PsiAccount* acc = controller_->contactList()->yandexTeamAccount();
	if (acc && acc->isAvailable()) {
		ui_.cb_account->setAccount(acc);
		// ui_.cb_server->setCurrentText(QString("conference.%1").arg(acc->jid().domain()));
	}
	else {
		ui_.cb_server->setCurrentText("conference.ya.ru");
		ui_.le_room->setText("test");
	}
#endif

	ui_.cb_server->setController(controller_);
	ui_.findRoomsServer->setController(controller_);

	ui_.recentRooms->setSortByOccupants(false);
	connect(ui_.recentRooms, SIGNAL(joinRoom(const XMPP::Jid&)), SLOT(joinRoom(const XMPP::Jid&)));
	connect(ui_.findRooms, SIGNAL(joinRoom(const XMPP::Jid&)), SLOT(joinRoom(const XMPP::Jid&)));

	connect(ui_.findRoomsFilter, SIGNAL(textChanged(const QString&)), ui_.findRooms, SLOT(setFilterText(const QString&)));
	connect(ui_.findRoomsServer, SIGNAL(activated(const QString&)), ui_.findRooms, SLOT(browseServer(const QString&)));

	connect(ui_.cb_account, SIGNAL(activated(PsiAccount*)), ui_.recentRooms, SLOT(setAccount(PsiAccount*)));
	connect(ui_.cb_account, SIGNAL(activated(PsiAccount*)), ui_.findRooms, SLOT(setAccount(PsiAccount*)));
	ui_.recentRooms->setAccount(ui_.cb_account->account());
	ui_.findRooms->setAccount(ui_.cb_account->account());

	foreach(QString j, controller_->recentGCList()) {
		ui_.recentRooms->addRoom(XMPP::Jid(j));
	}

	QTimer::singleShot(0, this, SLOT(browseServer()));
	moveToCenterOfScreen();
}

YaMucJoinDlg::~YaMucJoinDlg()
{
	if (controller_)
		controller_->dialogUnregister(this);
	if (account_)
		account_->dialogUnregister(this);
}

void YaMucJoinDlg::joinRoom(const XMPP::Jid& roomJid)
{
	if (!roomJid.isEmpty()) {
		setJid(roomJid);
		doJoin();
	}
}

void YaMucJoinDlg::setAccount(PsiAccount* account)
{
	ui_.cb_account->setAccount(account);
}

void YaMucJoinDlg::setJid(const XMPP::Jid& jid)
{
	ui_.cb_server->setCurrentText(jid.domain());
	ui_.le_room->setText(jid.node());
}

void YaMucJoinDlg::setNick(const QString nick)
{
	Q_UNUSED(nick);
}

void YaMucJoinDlg::setPassword(const QString& password)
{
	ui_.le_password->setText(password);
	ui_.ck_password->setChecked(!password.isEmpty());
}

void YaMucJoinDlg::joined()
{
	controller_->recentGCAdd(jid_.full());

	closeDialogs(this);
	deleteLater();
}

void YaMucJoinDlg::error(int errorCode, const QString& str)
{
	if (errorCode == 409) {
		++nicknameAttempt_;
		doJoin();
		return;
	}
	// ui_.busy->stop();
	// setWidgetsEnabled(true);

	if (account_)
		account_->dialogUnregister(this);
	controller_->dialogRegister(this);

	QMessageBox* msg = new QMessageBox(QMessageBox::Information, tr("Error"), tr("Unable to join groupchat.\nReason: %1").arg(str), QMessageBox::Ok, this);
	msg->setAttribute(Qt::WA_DeleteOnClose, true);
	msg->setModal(false);
	msg->show();
}

// void YaMucJoinDlg::done(int r)
// {
// 	if (ui_.busy->isActive()) {
// 		//int n = QMessageBox::information(0, tr("Warning"), tr("Are you sure you want to cancel joining groupchat?"), tr("&Yes"), tr("&No"));
// 		//if(n != 0)
// 		//	return;
// 		account_->groupChatLeave(jid_.domain(), jid_.node());
// 	}
// 	BaseMucJoinDlg::done(r);
// }

bool YaMucJoinDlg::doJoinBookmark()
{
	ui_.tabWidget->setCurrentWidget(ui_.tabCreate);
	return doJoin();
}

bool YaMucJoinDlg::doJoin()
{
	account_ = ui_.cb_account->account();
	if (!account_ || !account_->checkConnected(this))
		return false;

	QString host = ui_.cb_server->currentText();
	QString room = ui_.le_room->text();
	QString nick = account_->nick();
	QString pass = ui_.le_password->text();

	if (host.isEmpty() || room.isEmpty() || nick.isEmpty()) {
		QMessageBox::information(this, tr("Error"), tr("You must fill out the fields in order to join."));
		return false;
	}

	if (host == "conference.jabber.ru") {
		nick = nick.left(20);
	}

	if (nicknameAttempt_) {
		nick += QString("_%1").arg(nicknameAttempt_ + 1);
	}

	Jid j = room + '@' + host + '/' + nick;
	if (!j.isValid()) {
		QMessageBox::information(this, tr("Error"), tr("You entered an invalid room name."));
		return false;
	}

	GCMainDlg *gc = account_->findDialog<GCMainDlg*>(j.bare());
	if (gc) {
		gc->ensureTabbedCorrectly();
		gc->bringToFront();
		if (gc->isInactive()) {
			gc->reactivate();
		}
		joined();
		return true;
	}

	bool joined = false;
	if (account_->groupChatJoin(host, room, nick, pass, false)) {
		joined = true;
	}

	if (!joined) {
		account_->groupChatLeave(host, room);
		joined = account_->groupChatJoin(host, room, nick, pass, false);
	}

	if (!joined) {
		QMessageBox::information(this, tr("Error"), tr("You are in or joining this room already!"));
		return false;
	}

	controller_->dialogUnregister(this);
	jid_ = room + '@' + host + '/' + nick;
	account_->dialogRegister(this, jid_);

	// setWidgetsEnabled(false);
	// ui_.busy->start();

	return false;
}

void YaMucJoinDlg::accept()
{
	nicknameAttempt_ = 0;

	if (ui_.tabWidget->currentWidget() == ui_.tabRecent) {
		joinRoom(ui_.recentRooms->currentJid());
	}
	else if (ui_.tabWidget->currentWidget() == ui_.tabFind) {
		joinRoom(ui_.findRooms->currentJid());
	}
	else {
		Q_ASSERT(ui_.tabWidget->currentWidget() == ui_.tabCreate);
		doJoin();
	}
}

const YaWindowTheme& YaMucJoinDlg::theme() const
{
	return theme_;
}

void YaMucJoinDlg::updatePasswordVisibility()
{
	ui_.lb_password->setVisible(ui_.ck_password->isChecked());
	ui_.le_password->setVisible(ui_.ck_password->isChecked());
}

QString YaMucJoinDlg::title() const
{
	return ui_.le_title->text();
}

QString YaMucJoinDlg::password() const
{
	return ui_.le_password->text();
}

void YaMucJoinDlg::browseServer()
{
	ui_.findRooms->browseServer(ui_.findRoomsServer->currentText());
}
