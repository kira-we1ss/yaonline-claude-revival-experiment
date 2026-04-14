/*
 * yachatdlgshared.h
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

#ifndef YACHATDLGSHARED_H
#define YACHATDLGSHARED_H

#include <QObject>
#include <QPointer>

class PsiCon;

#include <QAction>
#include <QMenu>

class YaChatDlgShared : public QObject
{
	Q_OBJECT
public:
	QMenu* recentFilesMenu() const { return recentFilesMenu_; }
	QAction* uploadFileAction() const { return uploadFileAction_; }
	QAction* typographyAction() const { return typographyAction_; }
	QAction* emoticonsAction() const { return emoticonsAction_; }
	QAction* checkSpellingAction() const { return checkSpellingAction_; }
	QAction* sendButtonEnabledAction() const { return sendButtonEnabledAction_; }

	static YaChatDlgShared* instance(PsiCon* controller);

signals:
	void uploadFile();
	void uploadRecentFile(const QString& fileName, const QString& url, qint64 size);

private slots:
	void typographyActionTriggered(bool enabled);
	void emoticonsActionTriggered(bool enabled);
	void checkSpellingActionTriggered(bool enabled);
	void sendButtonEnabledActionTriggered(bool enabled);
	void optionChanged(const QString& option);
	void updateRecentFiles();
	void uploadRecentFile();
	void clearRecentFiles();

private:
	YaChatDlgShared(PsiCon* controller);
	~YaChatDlgShared();

	static YaChatDlgShared* instance_;
	QPointer<PsiCon> controller_;
	QPointer<QMenu> recentFilesMenu_;
	QPointer<QAction> uploadFileAction_;
	QPointer<QAction> clearRecentFilesAction_;
	QPointer<QAction> typographyAction_;
	QPointer<QAction> emoticonsAction_;
	QPointer<QAction> checkSpellingAction_;
	QPointer<QAction> sendButtonEnabledAction_;
};

#endif
