/*
 * yadebugconsole.h
 * Copyright (C) 2009  Yandex LLC (Michail Pishchagin)
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

#ifndef YADEBUGCONSOLE_H
#define YADEBUGCONSOLE_H

#include "yawindow.h"
#include <QPointer>
#include <QDateTime>

#include "tasklist.h"
#include "ui_yadebugconsole.h"
#include "yawindowtheme.h"

class PsiCon;
class MrimHelper;

class YaDebugConsole : public YaWindow
{
	Q_OBJECT
public:
	YaDebugConsole(PsiCon* controller);
	~YaDebugConsole();

	// reimplemented
	virtual const YaWindowTheme& theme() const;

public slots:
	void activate();

private slots:
	void clear();
	void detectInvisible();

	void registerMrim();
	void registerJ2j();
	void serviceDiscovery();
	void bookmarks();

	void detectInvisibleTaskFinished();

	void enableLoggingChanged();
	void logMessagesChanged();
	void sendLogs();

protected:
	void appendLog(const QString& message);

private:
	Ui::DebugConsole ui_;
	QPointer<PsiCon> controller_;
	YaWindowTheme theme_;

	QDateTime detectInvisibleStartTime_;
	TaskList taskList_;
};

#endif
