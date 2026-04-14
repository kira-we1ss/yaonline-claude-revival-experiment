/*
 * yamucjoin.h
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

#ifndef YAMUCJOIN_H
#define YAMUCJOIN_H

#include "mucjoindlg.h"

#include <QPointer>

#include "ui_yamucjoin.h"
#include "yawindowtheme.h"

class YaMucJoinDlg : public BaseMucJoinDlg
{
	Q_OBJECT
protected:
	YaMucJoinDlg(PsiCon* controller, PsiAccount* defaultAccount);
public:
	~YaMucJoinDlg();

	// reimplemented
	virtual void setAccount(PsiAccount* account);
	virtual void setJid(const XMPP::Jid& jid);
	virtual void setNick(const QString nick);
	virtual void setPassword(const QString& password);

	// reimplemented
	virtual QString title() const;
	virtual QString password() const;

	// reimplemented
	virtual void joined();
	virtual void error(int, const QString &);

	// reimplemented
	virtual const YaWindowTheme& theme() const;

public slots:
	// void done(int);
	bool doJoin();
	bool doJoinBookmark();

	// reimplemented
	void accept();

private slots:
	void updatePasswordVisibility();
	void joinRoom(const XMPP::Jid& roomJid);
	void browseServer();

private:
	Ui::YaMucJoin ui_;
	QPointer<PsiCon> controller_;
	QPointer<PsiAccount> account_;
	YaWindowTheme theme_;
	XMPP::Jid jid_;
	int nicknameAttempt_;

	friend class BaseMucJoinDlg;
};

#endif
