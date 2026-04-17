/*
 * chatdlg.h - dialog for handling chats
 * Copyright (C) 2001-2007  Justin Karneges, Michail Pishchagin
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

#ifndef CHATDLG_H
#define CHATDLG_H

#include "chatdlgbase.h"

#include "xmpp_receipts.h"
#include "xmpp_yadatetime.h"

namespace XMPP
{
class Jid;
class Message;
}
using namespace XMPP;

class PsiAccount;
class UserListItem;
class QDropEvent;
class QDragEnterEvent;
class QEvent;

class ChatDlg : public ChatDlgBase
{
	Q_OBJECT
protected:
	ChatDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager);
	virtual void init();

public:
	static ChatDlg* create(const Jid& jid, PsiAccount* account, TabManager* tabManager);
	~ChatDlg();

	// reimplemented
	void setJid(const Jid &);
	QString getDisplayName() const;

	static QSize defaultSize();
	bool readyToHide();

	// reimplemented
	virtual TabbableWidget::State state() const;
	virtual QString desiredCaption() const;
	virtual void invalidateTab();

	virtual void receivedPendingMessage();

signals:
	void aInfo(const Jid &);
	void aHistory(const Jid &);
	void aVoice(const Jid &);
	void messagesRead(const Jid &);
	void aSend(const Message &);
	void aFile(const Jid &);

	/**
	 * Signals if user (re)started/stopped composing
	 */
	void composing(bool);

protected:
	// reimplemented
	void closeEvent(QCloseEvent *);
	void resizeEvent(QResizeEvent *);
	void hideEvent(QHideEvent *);
	void showEvent(QShowEvent *);
	void changeEvent(QEvent* event);
	virtual void dropEvent(QDropEvent* event);
	void dragEnterEvent(QDragEnterEvent* event);

public slots:
	// reimplemented
	virtual void deactivated();
	virtual void activated();
	virtual void optionsUpdate();

	virtual void updateContact(const Jid &, bool);
	void incomingMessage(const Message &);
	virtual void updateAvatar() = 0;
	void updateAvatar(const Jid&);

protected slots:
	void doInfo();
	virtual void doHistory();
	virtual bool doSend();
	void sendMessage(XMPP::Message m, bool userAction);
	void doVoice();
	void doFile();

private slots:
	void setKeepOpenFalse();
	void setWarnSendFalse();
	virtual void updatePGP();
	virtual void setPGPEnabled(bool enabled);
	void encryptedMessageSent(int, bool, int);
	void slotScroll();
	void setChatState(XMPP::ChatState s);
	void updateIsComposing(bool);
	void setContactChatState(ChatState s);
	void capsChanged(const Jid&);
	void setComposing();
	void inactiveTimeout(); // XEP-0085: idle → StateInactive after 2 min

protected slots:
	void checkComposing();
	virtual void setLooks();

	// reimplemented
	virtual void chatEditCreated();

protected:
	// reimplemented
	virtual void doSendMessage(const XMPP::Message& m);

	void resetComposing();
	virtual void doneSend(const XMPP::Message& m);
	void setSelfDestruct(int);
	void deferredScroll();
	bool isEmoteText(const QString& text);
	bool isEmoteMessage(const XMPP::Message& m);
	QString messageText(const QString& text, bool isEmote, bool isHtml = false);
	QString messageText(const XMPP::Message& m);

	virtual void capsChanged();
	virtual void contactUpdated(UserListItem* u, int status, const QString& statusString);
	virtual QString colorString(bool local, SpooledType spooled) const = 0;

	void appendMessage(const Message &, bool local = false);
	virtual bool isEncryptionEnabled() const;
	virtual void appendSysMsg(const QString& txt) = 0;
#ifndef YAPSI
	virtual void appendEmoteMessage(SpooledType spooled, const QDateTime& time, bool local, QString txt) = 0;
	virtual void appendNormalMessage(SpooledType spooled, const QDateTime& time, bool local, QString txt) = 0;
#else
	virtual void appendEmoteMessage(SpooledType spooled, const QDateTime& time, bool local, int spamFlag, QString id, XMPP::MessageReceipt messageReceipt, QString txt, const XMPP::YaDateTime& yaTime, int yaFlags) = 0;
	virtual void appendNormalMessage(SpooledType spooled, const QDateTime& time, bool local, int spamFlag, QString id, XMPP::MessageReceipt messageReceipt, QString txt, const XMPP::YaDateTime& yaTime, int yaFlags) = 0;
#endif
	virtual void appendMessageFields(const Message& m) = 0;
	virtual void nicksChanged();

	QString whoNick(bool local) const;

	bool canChatState() const;

private:
	QString dispNick_;
	int status_;
	QString statusString_;

	bool keepOpen_;
	bool warnSend_;

	QString lastReceivedMsgId_; // XEP-0333: id of the last incoming message (for <displayed> marker)
	QString lastSentMsgId_;     // XEP-0308: id of last sent message (for correction)

	QTimer* selfDestruct_;

	QString key_;
	int transid_;
	Message m_;
	bool lastMessageUserAction_;
	bool lastWasEncrypted_;

	// Message Events & Chat States
	QTimer* composingTimer_;
	QTimer* inactiveTimer_; // XEP-0085: fires after 120s of no input → StateInactive
	bool isComposing_;
	bool sendComposingEvents_;
	QString eventId_;
	ChatState contactChatState_;
	ChatState lastChatState_;
};

#endif
