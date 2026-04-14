/*
 * chatdlgbase.h
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

#ifndef CHATDLGBASE_H
#define CHATDLGBASE_H

#include "tabbablewidget.h"

#ifdef YAPSI
class YaChatView;
typedef YaChatView ChatViewClass;
#else
class ChatView;
typedef ChatView ChatViewClass;
#endif
class ChatEdit;
class YaChatViewModel;

class ChatDlgBase : public TabbableWidget
{
	Q_OBJECT
protected:
	ChatDlgBase(const Jid& jid, PsiAccount* account, TabManager* tabManager);
	virtual void init();

public:
	~ChatDlgBase();

	void uploadFile(const QString& fileName);
	void uploadFiles(const QStringList& fileNames);

	// reimplemented
	virtual QString desiredCaption() const;
	int unreadMessageCount() const;
	void setUnreadMessageCount(int pending);

	enum SpooledType {
		Spooled_None,
		Spooled_OfflineStorage,
		Spooled_History,

		Spooled_Sync
	};

public slots:
	virtual void doClear();
	virtual void optionsUpdate();

	// reimplemented
	virtual void activated();

protected slots:
	virtual bool doSend();
	void scrollUp();
	void scrollDown();
	void updateSendAction();
	virtual void chatEditCreated();
	virtual void optionChanged(const QString& option);
	virtual void updateModelNotices();

private slots:
	void initComposing();
	void addEmoticon(QString text);

	void uploadFile();
	void uploadRecentFile(const QString& fileName, const QString& url, qint64 size);
	void uploadFinished(const QString& fileName, const QString& url, qint64 size);

	void uploadFileStarted(const QString& id);

protected:
	// reimplemented
	void keyPressEvent(QKeyEvent *);
	bool eventFilter(QObject *obj, QEvent *event);
	void showEvent(QShowEvent*);

	virtual void setShortcuts();
	virtual void initUi() = 0;
	virtual void doSendMessage(const XMPP::Message& m) = 0;
	virtual ChatViewClass* chatView() const = 0;
	virtual ChatEdit* chatEdit() const = 0;
#ifdef YAPSI
	virtual YaChatViewModel* model() const;
#endif
	virtual void setLooks();

	QAction* actionSend() const;
	virtual bool couldSendMessages() const;
	bool highlightersInstalled() const;

private:
	bool highlightersInstalled_;
#ifdef YAPSI
	YaChatViewModel* model_;
#endif

	void initActions();
	QAction* act_send_;
	QAction* act_scrollup_;
	QAction* act_scrolldown_;
	QAction* act_close_;
	int pending_;
};

#endif
