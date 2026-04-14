/*
 * yagroupchatdlg.h
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

#ifndef YAGROUPCHATDLG_H
#define YAGROUPCHATDLG_H

#include "groupchatdlg.h"
#include <QPointer>

#include "ui_yagroupchatdialog.h"

class YaGroupchatContactListModel;
namespace XMPP {
	class DiscoInfoTask;
};

class YaGroupchatDlg : public GCMainDlg
{
	Q_OBJECT
public:
	YaGroupchatDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager);
	~YaGroupchatDlg();

	// reimplemented
	virtual void error(int, const QString &);
	virtual void presence(const QString &, const Status &);
	virtual void message(const Message &);
	virtual void setTitle(const QString& title);
	virtual void setBookmarkName(const QString& bookmarkName);

	// reimplemented
	virtual QString getDisplayName() const;
	virtual void ban(const QString& nick);
	virtual void changeRole(const QString& nick, XMPP::MUCItem::Role role);

	// reimplemented
	virtual QStringList nickList() const;

protected:
	// reimplemented
	virtual void configDlgUpdateSelfAffiliation();
	virtual void setConfigureEnabled(bool enabled);
	virtual void subjectChanged(const QString& subject);
	virtual void deactivated();
	virtual void activated();
	virtual void appendSysMsg(const QString& str, bool alert, const QDateTime &ts=QDateTime());
	virtual void chatEditCreated();
	virtual void doJoined();

private slots:
	void toggleFavorite();
	void editSubject();
	void updateFavorite();
	void subjectChanged();
	void showContactProfile();
	void showAlternateContactProfile();
	void doTrackbar();
	void discoInfoFinished();
	void updateRoomTitle();

private:
	// reimplemented
	ChatViewClass* chatView() const;
	ChatEdit* chatEdit() const;

	Ui::YaGroupchatDlg ui_;
	YaGroupchatContactListModel* contactList_;
	bool doTrackbar_;
	bool contactStatusRecreated_;
	QPointer<XMPP::DiscoInfoTask> getRoomTitleTask_;
	bool subjectCanBeModified_;
	QString title_;
	QString bookmarkName_;

	virtual void initUi();
	void recreateContactStatus();
};

#endif
