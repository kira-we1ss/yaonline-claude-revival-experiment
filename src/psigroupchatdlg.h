/*
 * psigroupchatdlg.h
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

#ifndef PSIGROUPCHATDLG_H
#define PSIGROUPCHATDLG_H

#include "groupchatdlg.h"
#include <QDialog>

#include "ui_groupchatdlg.h"

class PsiOptions;

class PsiGroupchatDlg : public GCMainDlg
{
	Q_OBJECT
public:
	PsiGroupchatDlg(const Jid& jid, PsiAccount* account, TabManager* tabManager);
	~PsiGroupchatDlg();

	// reimplemented
	virtual void error(int, const QString &);
	virtual void presence(const QString &, const Status &);
	virtual void message(const Message &);

	// reimplemented
	virtual QString desiredCaption() const;

protected:
	// reimplemented
	virtual void initUi();
	virtual bool doSend();
	virtual void appendSysMsg(const QString& str, bool alert, const QDateTime &ts=QDateTime());
	virtual void configDlgUpdateSelfAffiliation();
	virtual void setConfigureEnabled(bool enabled);

	// reimplemented
	void dragEnterEvent(QDragEnterEvent *);
	void dropEvent(QDropEvent *);
	void resizeEvent(QResizeEvent*);

public slots:
	// reimplemented
	virtual void deactivated();
	virtual void activated();

	void optionsUpdate();

private slots:
	// reimplemented
	virtual bool doDisconnect();
	virtual bool doConnect();
	virtual void doJoined();

	void doTopic();
	void openFind();
	void doFind(const QString &);
	void lv_action(const QString &, const Status &, int);
	void doClear();
	void doClearButton();
	void buildMenu();
	void updateIdentityVisibility();
#ifdef WHITEBOARDING
	void openWhiteboard();
#endif
	void chatEditCreated();

public:
	class Private;
	friend class Private;
private:
	Private *d;
	Ui::GroupChatDlg ui_;

	void doAlert();
	void appendMessage(const Message &, bool);
	void setLooks();

	void contextMenuEvent(QContextMenuEvent *);

	QString getNickColor(QString);
	QMap<QString,int> nicks;
	int nicknumber;
	PsiOptions* options_;
};

class GCFindDlg : public QDialog
{
	Q_OBJECT
public:
	GCFindDlg(const QString &, QWidget *parent=0, const char *name=0);
	~GCFindDlg();

	void found();
	void error(const QString &);

signals:
	void find(const QString &);

private slots:
	void doFind();

private:
	QLineEdit *le_input;
};

#endif
