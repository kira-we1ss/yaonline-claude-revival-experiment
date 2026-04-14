/*
 * yalogeventsmanager.h
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

#ifndef YALOGEVENTSMANAGER_H
#define YALOGEVENTSMANAGER_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QPointer>

class QTimer;
class PsiAccount;
class PsiCon;
class PsiEvent;

#include "xmpp_jid.h"
#include "xmpp_yadatetime.h"
#include "xmpp_tasks.h"

class YaLogEventsManager : public QObject
{
	Q_OBJECT
public:
	YaLogEventsManager(PsiCon* parent);
	~YaLogEventsManager();

	void logEventOnServer(const XMPP::Jid& j, PsiEvent* e, const PsiAccount* account);

private slots:
	void writeToDisk();
	void load();
	void save();

	void logEventsOnServer();
	void logMessageFinished();

protected:
	QString fileName() const;

private:
	struct LogEventRequest {
		XMPP::Jid accountJid;
		bool originLocal;
		XMPP::YaDateTime timeStamp;
		XMPP::Message message;

		QPointer<XMPP::JT_YaLogMessage> task;
	};

	PsiCon* controller_;
	QTimer* writeToDiskTimer_;
	QTimer* logEventRequestsTimer_;
	QList<LogEventRequest*> logEventRequests_;
	QHash<XMPP::JT_YaLogMessage*, LogEventRequest*> logEventRequestTasks_;
};

#endif
