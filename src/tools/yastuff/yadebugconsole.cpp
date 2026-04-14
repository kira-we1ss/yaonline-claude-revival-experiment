/*
 * yadebugconsole.cpp
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

#include "yadebugconsole.h"

#include <QScrollBar>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDesktopServices>

#include "common.h"
#include "psiaccount.h"
#include "psicon.h"
#include "psicontact.h"
#include "psicontactlist.h"
#include "lastactivitytask.h"
#include "yatransportmanager.h"
#include "psilogger.h"

#ifdef Q_WS_WIN
#if __GNUC__ >= 3
#	define WINVER    0x0500
#	define _WIN32_IE 0x0500
#endif
#include <windows.h>
#include <shlobj.h>
#endif

YaDebugConsole::YaDebugConsole(PsiCon* controller)
	: YaWindow()
	, controller_(controller)
	, theme_(YaWindowTheme::Roster)
{
	ui_.setupUi(this);
	updateContentsMargins(0, 0, 0, 0);
	ui_.account->setController(controller);
	ui_.account->setOnlineOnly(true);

	ui_.enableLogging->setChecked(PsiLogger::isLoggingEnabled());
	ui_.logMessages->setChecked(PsiLogger::isLogAllMessagesEnabled());

	connect(ui_.enableLogging, SIGNAL(stateChanged(int)), SLOT(enableLoggingChanged()));
	connect(ui_.logMessages, SIGNAL(stateChanged(int)), SLOT(logMessagesChanged()));
	connect(ui_.sendLogs, SIGNAL(clicked()), SLOT(sendLogs()));

	connect(ui_.serviceDiscovery, SIGNAL(clicked()), SLOT(serviceDiscovery()));
	connect(ui_.bookmarks, SIGNAL(clicked()), SLOT(bookmarks()));

#ifdef YAPSI_ACTIVEX_SERVER
	ui_.gbMail->hide();
	// ui_.gbJ2j->hide();
#else
	connect(ui_.mrimButton, SIGNAL(clicked()), SLOT(registerMrim()));
	connect(ui_.j2jButton, SIGNAL(clicked()), SLOT(registerJ2j()));
#endif

	connect(ui_.clear, SIGNAL(clicked()), SLOT(clear()));
	connect(ui_.detectInvisible, SIGNAL(clicked()), SLOT(detectInvisible()));
}

YaDebugConsole::~YaDebugConsole()
{
}

void YaDebugConsole::clear()
{
	ui_.textEdit->clear();
}

void YaDebugConsole::detectInvisible()
{
	PsiAccount* account = controller_->contactList()->yaServerHistoryAccount();
	if (!account || !account->isAvailable())
		return;

	taskList_.clear();
	detectInvisibleStartTime_ = QDateTime::currentDateTime().addSecs(-(60 * 10));
	appendLog(QString("detectInvisible started %1").arg(detectInvisibleStartTime_.toString(Qt::ISODate)));

	foreach(PsiContact* c, account->contactList()) {
		if (!c->isOnline()) {
			// appendLog(QString("%1 is offline").arg(c->jid().full()));

			LastActivityTask* jtLast = new LastActivityTask(c->jid().bare(), account->client()->rootTask());
			connect(jtLast, SIGNAL(finished()), SLOT(detectInvisibleTaskFinished()));
			jtLast->go(true);
			taskList_.append(jtLast);
		}
	}
}

void YaDebugConsole::detectInvisibleTaskFinished()
{
	LastActivityTask *j = static_cast<LastActivityTask*>(sender());
	if (j->success()) {
		if (detectInvisibleStartTime_ <= j->time()) {
			appendLog(QString("finished %1 %2")
			.arg(j->time().toString(Qt::ISODate))
			.arg(j->jid().full())
			);
		}
	}
}

void YaDebugConsole::activate()
{
	::bringToFront(this);
}

void YaDebugConsole::appendLog(const QString& message)
{
	QDateTime now = QDateTime::currentDateTime();
	QString text = QString("[%1] %2").arg(now.toString("hh:mm:ss")).arg(message);
	ui_.textEdit->insertHtml(text);
	ui_.textEdit->insertHtml("<br>");
	ui_.textEdit->verticalScrollBar()->setValue(ui_.textEdit->verticalScrollBar()->maximum());
}

void YaDebugConsole::serviceDiscovery()
{
	PsiAccount* account = ui_.account->account();
	if (account) {
		account->actionDisco(ui_.account->account()->jid().domain(), QString());
	}
}

void YaDebugConsole::bookmarks()
{
	PsiAccount* account = ui_.account->account();
	if (account) {
		account->actionManageBookmarks();
	}
}

const YaWindowTheme& YaDebugConsole::theme() const
{
	return theme_;
}

void YaDebugConsole::registerMrim()
{
	YaTransport* transport = controller_->yaTransportManager()->findTransport("MRIM");
	if (transport) {
		transport->registerTransport(ui_.mrimLogin->text(), ui_.mrimPass->text());
	}
}

void YaDebugConsole::registerJ2j()
{
	YaTransport* transport = controller_->yaTransportManager()->findTransport("J2J");
	if (transport) {
		transport->registerTransport(ui_.j2jLogin->text(), ui_.j2jPass->text());
	}
}

void YaDebugConsole::enableLoggingChanged()
{
	PsiLogger::setLoggingEnabled(ui_.enableLogging->isChecked());
}

void YaDebugConsole::logMessagesChanged()
{
	PsiLogger::setLogAllMessagesEnabled(ui_.logMessages->isChecked());
}

void YaDebugConsole::sendLogs()
{
	QStringList fileNames;
#ifdef YAPSI_ACTIVEX_SERVER
	{
		QString base = QDir::homePath();
		WCHAR str[MAX_PATH+1] = { 0 };
		if (SHGetSpecialFolderPathW(0, str, CSIDL_LOCAL_APPDATA, true))
			base = QString::fromWCharArray(str);
		fileNames << base + "/Yandex/Online/online.log";
	}
#endif
	fileNames << PsiLogger::logFileName();

	QString tempDirName = QString("online-log-%1")
	                      .arg(QDateTime::currentDateTime().toUTC().toString("yyyy-MM-dd-HHmmss"));
	QDir tempDir(QDir::tempPath());
	tempDir.mkdir(tempDirName);

	tempDirName = QDir::tempPath() + "/" + tempDirName;

	foreach(const QString& f, fileNames) {
		QFileInfo fi(f);
		QFile::copy(f, tempDirName + "/" + fi.fileName());
	}

	qWarning("written logs to %s", qPrintable(tempDirName));
	QDesktopServices::openUrl(QUrl("file:///" + tempDirName, QUrl::TolerantMode));
}
