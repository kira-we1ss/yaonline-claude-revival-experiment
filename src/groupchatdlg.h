/*
 * groupchatdlg.h - dialogs for handling groupchat
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

#ifndef GROUPCHATDLG_H
#define GROUPCHATDLG_H

#include "chatdlgbase.h"
#include <QHash>
#include <QPointer>

using namespace XMPP;

class MUCConfigDlg;

#include "mucmanager.h"
#include "xmpp_message.h"

class TabCompletionMUC;
class PsiEvent;

class GCMainDlg : public ChatDlgBase
{
	Q_OBJECT
public:
	static GCMainDlg* create(const Jid& jid, PsiAccount* account, TabManager* tabManager);
	~GCMainDlg();

	static bool mucEnabled();

	virtual void error(int, const QString &);
	virtual void presence(const QString &, const Status &);
	virtual void message(const Message &);
	virtual void joined();
	virtual void setPassword(const QString&);
	virtual void setTitle(const QString& title);
	virtual void setBookmarkName(const QString& bookmarkName);

	bool configureEnabled() const;
	virtual QStringList nickList() const = 0;
	QString nick() const;
	QString lastReferrer() const;

	void openChat(const QString& nick);
	void kick(const QString& nick);
	virtual void ban(const QString& nick) = 0;
	void vCard(const QString& nick);
	virtual void changeRole(const QString& nick, XMPP::MUCItem::Role role) = 0;

	bool isQuitting() const;
	bool isConnected() const;
	bool isInactive() const;
	void reactivate();

	// reimplemented
	virtual TabbableWidget::State state() const;

	// XEP-0384 OMEMO MUC: nick → real bare JID accessor (populated from MUC presence)
	XMPP::Jid realJidForNick(const QString& nick) const;
	const QHash<QString, XMPP::Jid>& nickToRealJidMap() const;

signals:
	void aSend(const Message &);
	void configureEnabledChanged();
	void gcStateChanged();
	void eventCreated(PsiEvent*);

public slots:
	void insertNick(const QString& nick);

protected slots:
	// reimplemented
	virtual bool doSend();
	virtual void chatEditCreated();

	void account_updatedActivity();
	void setConnecting();
	void unsetConnecting();
	void action_error(MUCManager::Action, int, const QString&);

protected:
	GCMainDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager);

	void closeEvent(QCloseEvent *);

	// reimplemented
	virtual void setLooks();
	virtual void init();
	virtual bool couldSendMessages() const;
	virtual bool eventFilter(QObject* obj, QEvent* e);
	virtual void doSendMessage(const XMPP::Message& m);
	void windowActivationChange(bool);

	virtual void appendSysMsg(const QString& str, bool alert, const QDateTime &ts=QDateTime());
	void mucKickMsgHelper(const QString &nick, const Status &s, const QString &nickJid, const QString &title,
	                      const QString &youSimple, const QString &youBy, const QString &someoneSimple,
	                      const QString &someoneBy);

	bool isConnecting() const;
	void nickChangeFailure();
	void setNick(const QString& nick);
	virtual bool doDisconnect();
	virtual void doForcedLeave();
	virtual bool doConnect();
	virtual void doJoined();
	virtual void setSubject(const QString& subject);
	QString currentSubject() const;
	virtual void subjectChanged(const QString& subject);

	QString password() const;

	enum GC_State {
		GC_Quitting,
		GC_Idle,
		GC_Connecting,
		GC_Connected,
		GC_ForcedLeave
	};

	virtual void setGcState(GC_State state);
	GC_State gcState() const;

	MUCManager* mucManager() const;
	MUCConfigDlg* configDlg() const;
	void configureRoom();
	virtual void configDlgUpdateSelfAffiliation() = 0;
	virtual void setConfigureEnabled(bool enabled);

	void appendMucMessage(const XMPP::Message& msg);
	bool hasMucMessage(const XMPP::Message& msg);

	bool nonAnonymous() const;
	void setNonAnonymous(bool nonAnonymous);

	void setLastReferrer(const QString& referer);

	void mucInfoDialog(const QString& title, const QString& message, const Jid& actor, const QString& reason);

	// XEP-0384 OMEMO MUC: nick → real bare JID (populated from <x muc#user><item jid=.../>)
	// Protected so YaGroupchatDlg (and other subclasses) can write to it in presence().
	QHash<QString, XMPP::Jid> nickToRealJid_;

private:
	GC_State gcState_;
	bool connecting_;
	QString password_;
	QString nick_;
	QString nickPrev_;
	MUCManager* mucManager_;
	QPointer<MUCConfigDlg> configDlg_;
	bool nonAnonymous_; // got status code 100 ?
	QString lastReferrer_;  // contains nick of last person, who have said "yourNick: ..."
	TabCompletionMUC* tabCompletion_;
	bool configureEnabled_;
	QString currentSubject_;

	struct MucMessage {
		QString nick;
		QString id;
		QString body;
		QString subject;
	};
	QList<MucMessage> mucMessages_;
};

#endif
