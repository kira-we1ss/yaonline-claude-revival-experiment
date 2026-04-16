/*
 * groupchatdlg.cpp - dialogs for handling groupchat
 * Copyright (C) 2001, 2002  Justin Karneges
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

#include "groupchatdlg.h"

#include <QMessageBox>

#include "psioptions.h"
#ifndef YAPSI
#include "psigroupchatdlg.h"
#else
#include "yagroupchatdlg.h"
#endif
#include "psiaccount.h"
#include "msgmle.h"
#include "mucmanager.h"
#include "mucconfigdlg.h"
#include "common.h"
#include "yachatviewmodel.h"
#include "iconset.h"
#include "tabcompletionmuc.h"

GCMainDlg* GCMainDlg::create(const Jid& jid, PsiAccount* account, TabManager* tabManager)
{
#ifndef YAPSI
	GCMainDlg* dlg = new PsiGroupchatDlg(jid, tabManager);
#else
	GCMainDlg* dlg = new YaGroupchatDlg(jid, account, tabManager);
#endif
	dlg->init();
	return dlg;
}

GCMainDlg::~GCMainDlg()
{
	doForcedLeave();
	setGcState(GC_Quitting);
}

bool GCMainDlg::mucEnabled()
{
	return true;
}

GCMainDlg::GCMainDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager)
	: ChatDlgBase(jid.userHost(), account, tabManager)
	, gcState_(GC_Idle)
	, connecting_(false)
	, nonAnonymous_(false)
	, configureEnabled_(false)
{
	nick_ = nickPrev_ = jid.resource();
	connect(account, SIGNAL(updatedActivity()), SLOT(account_updatedActivity()));

	mucManager_ = new MUCManager(account->client(), this->jid(), this);
	connect(mucManager_, SIGNAL(action_error(MUCManager::Action, int, const QString&)), SLOT(action_error(MUCManager::Action, int, const QString&)));

	tabCompletion_ = new TabCompletionMUC(this);
}

void GCMainDlg::error(int, const QString &)
{
}

void GCMainDlg::mucKickMsgHelper(const QString &nick, const Status &s, const QString &nickJid, const QString &title,
                                 const QString &youSimple, const QString &youBy, const QString &someoneSimple,
                                 const QString &someoneBy)
{
	QString message;
	if (nick == this->nick()) {
		message = youSimple;
		mucInfoDialog(title, message, s.mucItem().actor(), s.mucItem().reason());
		if (!s.mucItem().actor().isEmpty()) {
			message = youBy.arg(s.mucItem().actor().full());
		}
		doForcedLeave();
	}
	else if (!s.mucItem().actor().isEmpty()) {
		message = someoneBy.arg(nickJid, s.mucItem().actor().full());
	}
	else {
		message = someoneSimple.arg(nickJid);
	}

	if (!s.mucItem().reason().isEmpty()) {
		message += QString(" (%1)").arg(s.mucItem().reason());
	}
	appendSysMsg(message, false, QDateTime::currentDateTime());
}

void GCMainDlg::presence(const QString& nick, const Status& status)
{
	Q_UNUSED(nick);
	Q_UNUSED(status);
}

void GCMainDlg::message(const Message &)
{
}

void GCMainDlg::joined()
{
	if (gcState_ == GC_Connecting) {
		setGcState(GC_Connected);
		doJoined();
		setConnecting();
		updateSendAction();
	}
}

void GCMainDlg::doJoined()
{
}

void GCMainDlg::setPassword(const QString& p)
{
	password_ = p;
	mucManager()->setRoomPassword(password_);
}

QString GCMainDlg::password() const
{
	return password_;
}

TabbableWidget::State GCMainDlg::state() const
{
	return TabbableWidget::StateNone;
}

void GCMainDlg::init()
{
	setAttribute(Qt::WA_DeleteOnClose);
	ChatDlgBase::init();

	X11WM_CLASS("groupchat");

	setConfigureEnabled(configureEnabled());
	setGcState(GC_Connected);
	doJoined();
	setConnecting();
}

void GCMainDlg::setLooks()
{
	ChatDlgBase::setLooks();

	setWindowOpacity(double(qMax(MINIMUM_OPACITY,PsiOptions::instance()->getOption("options.ui.chat.opacity").toInt()))/100);

	// update the widget icon
#ifndef Q_OS_MAC
	setWindowIcon(IconsetFactory::icon("psi/groupChat").icon());
#endif
}

GCMainDlg::GC_State GCMainDlg::gcState() const
{
	return gcState_;
}

void GCMainDlg::setGcState(GCMainDlg::GC_State state)
{
	if (gcState_ != state) {
		gcState_ = state;
		updateSendAction();
		emit gcStateChanged();
	}
}

QString GCMainDlg::nick() const
{
	return nick_;
}

void GCMainDlg::setNick(const QString& nick)
{
	nickPrev_ = nick_;
	nick_ = nick;
	account()->groupChatChangeNick(jid().host(), jid().user(), nick, account()->status());
}

void GCMainDlg::nickChangeFailure()
{
	nick_ = nickPrev_;
}

bool GCMainDlg::doSend()
{
	if (!ChatDlgBase::doSend())
		return false;

	QString str = chatEdit()->toPlainText();
	if (str == "/clear") {
		doClear();
		chatEdit()->clear();
		return false;
	}

	if (str.toLower().startsWith("/nick ")) {
		QString nick = str.mid(6).trimmed();
		if (!nick.isEmpty()) {
			setNick(nick);
		}
		chatEdit()->clear();
		return false;
	}

	if (gcState_ != GC_Connected)
		return false;

	Message m(jid());
	m.setBody(str);
	m.setTimeStamp(QDateTime::currentDateTime());

	doSendMessage(m);

	chatEdit()->clear();
	return true;
}

void GCMainDlg::doSendMessage(const XMPP::Message& _m)
{
	XMPP::Message m = _m;
	m.setType("groupchat");
	emit aSend(m);
}

bool GCMainDlg::couldSendMessages() const
{
	return ChatDlgBase::couldSendMessages() &&
	       gcState_ == GC_Connected;
}

bool GCMainDlg::doDisconnect()
{
	if (gcState_ != GC_Idle && gcState_ != GC_ForcedLeave) {
		setGcState(GC_Idle);
		return true;
	}
	return false;
}

void GCMainDlg::doForcedLeave() {
	if(gcState_ != GC_Idle && gcState_ != GC_ForcedLeave) {
		doDisconnect();
		account()->groupChatLeave(jid().domain(), jid().node());
		setGcState(GC_ForcedLeave);
	}
}

bool GCMainDlg::doConnect()
{
	if (gcState_ == GC_Idle) {
		setGcState(GC_Connecting);

		QString host = jid().host();
		QString room = jid().user();

		if (account()->groupChatJoin(host, room, nick(), password())) {
			return true;
		}

		// appendSysMsg(tr("Error: You are in or joining this room already!"), true);
		setGcState(GC_Idle);
	}
	return false;
}

void GCMainDlg::account_updatedActivity()
{
	if (!account()->loggedIn()) {
		doDisconnect();
	}
	else {
		if (gcState_ == GC_Idle) {
			doConnect();
		}
		else if (gcState_ == GC_Connected) {
			Status s = account()->status();
			s.setXSigned("");
			account()->groupChatSetStatus(jid().host(), jid().user(), s);
		}
	}
}

bool GCMainDlg::isQuitting() const
{
	return gcState_ == GC_Quitting;
}

bool GCMainDlg::isConnecting() const
{
	return connecting_;
}

void GCMainDlg::setConnecting()
{
	connecting_ = true;
	QTimer::singleShot(5000, this, SLOT(unsetConnecting()));
}

void GCMainDlg::unsetConnecting()
{
	connecting_ = false;
}

void GCMainDlg::closeEvent(QCloseEvent *e)
{
	e->accept();
}

void GCMainDlg::appendSysMsg(const QString& str, bool alert, const QDateTime &ts)
{
	Q_UNUSED(alert);
	QDateTime timeStamp = ts;
	if (timeStamp.isNull() || !timeStamp.isValid()) {
		timeStamp = QDateTime::currentDateTime();
	}
	model()->addGroupchatMessage(static_cast<YaChatViewModel::SpooledType>(Spooled_None), timeStamp, QString(), -1, str, false, QString(), YaChatViewModel::GC_SystemMessage);
}

MUCConfigDlg* GCMainDlg::configDlg() const
{
	return configDlg_;
}

MUCManager* GCMainDlg::mucManager() const
{
	return mucManager_;
}

void GCMainDlg::configureRoom()
{
	if (configDlg_)
		::bringToFront(configDlg_);
	else {
		configDlg_ = new MUCConfigDlg(mucManager(), this);
#ifdef YAPSI
		configDlg_->init();
#endif
		configDlgUpdateSelfAffiliation();
		configDlg_->show();
	}
}

void GCMainDlg::action_error(MUCManager::Action, int, const QString& err)
{
	appendSysMsg(err, false);
}

bool GCMainDlg::configureEnabled() const
{
	return configureEnabled_;
}

void GCMainDlg::setConfigureEnabled(bool enabled)
{
	configureEnabled_ = enabled;
	emit configureEnabledChanged();
}

void GCMainDlg::mucInfoDialog(const QString& title, const QString& message, const Jid& actor, const QString& reason)
{
	QString m = message;

	if (!actor.isEmpty())
		m += tr(" by %1").arg(actor.full());
	m += ".";

	if (!reason.isEmpty())
		m += tr("\nReason: %1").arg(reason);

	QMessageBox::information(this, title, m);
}

bool GCMainDlg::nonAnonymous() const
{
	return nonAnonymous_;
}

void GCMainDlg::setNonAnonymous(bool nonAnonymous)
{
	nonAnonymous_ = nonAnonymous;
}

bool GCMainDlg::hasMucMessage(const XMPP::Message& msg)
{
	foreach(MucMessage m, mucMessages_) {
		if (msg.body() == m.body &&
		    msg.subject() == m.subject &&
		    msg.id() == m.id &&
		    msg.from().resource() == m.nick)
		{
			return true;
		}
	}
	return false;
}

void GCMainDlg::appendMucMessage(const XMPP::Message& msg)
{
	MucMessage m;
	m.body = msg.body();
	m.subject = msg.subject();
	m.id = msg.id();
	m.nick = msg.from().resource();
	mucMessages_.append(m);

	if (mucMessages_.count() > 100) {
		mucMessages_.takeFirst();
	}
}

void GCMainDlg::setSubject(const QString& subject)
{
	Message m(jid());
	m.setType("groupchat");
	m.setSubject(subject);
	m.setTimeStamp(QDateTime::currentDateTime());
	emit aSend(m);
}

QString GCMainDlg::lastReferrer() const
{
	return lastReferrer_;
}

void GCMainDlg::setLastReferrer(const QString& referer)
{
	lastReferrer_ = referer;
}

void GCMainDlg::insertNick(const QString& nick)
{
	if (nick.isEmpty())
		return;

	QString text = nick;
	QTextCursor cursor = chatEdit()->textCursor();
	bool atStart = chatEdit()->toPlainText().isEmpty() || cursor.position() == 0;
	if (atStart) {
		text += tabCompletion_->nickSeparator;
	}
	text += " ";

	cursor.beginEditBlock();
	cursor.insertText(text);
	cursor.endEditBlock();

	chatEdit()->setTextCursor(cursor);
}

void GCMainDlg::chatEditCreated()
{
	ChatDlgBase::chatEditCreated();
	tabCompletion_->setTextEdit(chatEdit());
	chatEdit()->installEventFilter(this);
}

bool GCMainDlg::eventFilter(QObject* obj, QEvent* e)
{
	if (obj == chatEdit() && e->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);

		if (keyEvent->key() == Qt::Key_Tab) {
			tabCompletion_->tryComplete();
			return true;
		}

		tabCompletion_->reset();
	}

	return ChatDlgBase::eventFilter(obj, e);
}

void GCMainDlg::openChat(const QString& nick)
{
	account()->invokeGCChat(jid().withResource(nick));
}

void GCMainDlg::kick(const QString& nick)
{
	mucManager()->kick(nick);
}

void GCMainDlg::vCard(const QString& nick)
{
	account()->invokeGCInfo(jid().withResource(nick));
}

bool GCMainDlg::isConnected() const
{
	return gcState() == GC_Connected;
}

bool GCMainDlg::isInactive() const
{
	return gcState() == GC_ForcedLeave;
}

void GCMainDlg::reactivate()
{
	setGcState(GC_Idle);
	doConnect();
}

QString GCMainDlg::currentSubject() const
{
	return currentSubject_;
}

void GCMainDlg::subjectChanged(const QString& subject)
{
	currentSubject_ = subject;
}

void GCMainDlg::windowActivationChange(bool oldstate)
{
	ChatDlgBase::windowActivationChange(oldstate);

	if (isActiveTab()) {
		activated();
	}
	else {
		deactivated();
	}
}

void GCMainDlg::setTitle(const QString& title)
{
	mucManager()->setRoomTitle(title);
}

void GCMainDlg::setBookmarkName(const QString& bookmarkName)
{
	Q_UNUSED(bookmarkName);
}
