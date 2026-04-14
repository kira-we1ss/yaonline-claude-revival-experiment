/*
 * yalogeventsmanager.cpp
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

#include "yalogeventsmanager.h"

#include <QTimer>

#include "psicon.h"
#include "psiaccount.h"
#include "psievent.h"
#include "psicontactlist.h"
#include "dummystream.h"
#include "xmpp_xmlcommon.h"
#include "atomicxmlfile.h"
#include "psilogger.h"

YaLogEventsManager::YaLogEventsManager(PsiCon* parent)
	: QObject(parent)
	, controller_(parent)
{
	writeToDiskTimer_ = new QTimer(this);
	writeToDiskTimer_->setSingleShot(false);
	writeToDiskTimer_->setInterval(10 * 1000);
	connect(writeToDiskTimer_, SIGNAL(timeout()), SLOT(writeToDisk()));

	logEventRequestsTimer_ = new QTimer(this);
	logEventRequestsTimer_->setSingleShot(true);
	logEventRequestsTimer_->setInterval(5 * 1000);
	connect(logEventRequestsTimer_, SIGNAL(timeout()), SLOT(logEventsOnServer()));

	load();
	logEventsOnServer();
}

YaLogEventsManager::~YaLogEventsManager()
{
	save();

	qDeleteAll(logEventRequests_);
	logEventRequests_.clear();
	logEventRequestTasks_.clear();
}

void YaLogEventsManager::writeToDisk()
{
	writeToDiskTimer_->stop();
	save();
}

QString YaLogEventsManager::fileName() const
{
	return pathToProfile(activeProfile) + "/yalogevents.xml";
}

// TODO: QXmlStreamReader
void YaLogEventsManager::load()
{
	QDomDocument doc;

	AtomicXmlFile f(fileName());
	if (!f.loadDocument(&doc))
		return;

	QDomElement root = doc.documentElement();
	if (root.tagName() != "yalogevents")
		return;

	if (root.attribute("version") != "1.0")
		return;

	DummyStream stream;
	for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement c = n.toElement();
		if (c.isNull())
			continue;

		if (c.tagName() == "i") {
			LogEventRequest* request = new LogEventRequest();
			request->accountJid = XMPP::Jid(c.attribute("account"));
			XMLHelper::readBoolAttribute(c, "originLocal", &request->originLocal);
			request->timeStamp = YaDateTime::fromYaIsoTime(c.attribute("timeStamp"));

			bool found = false;
			QDomElement msg = findSubTag(c, "message", &found);
			if ( found ) {
				Stanza s = stream.createStanza(addCorrectNS(msg));
				request->message.fromStanza(s, 0);
			}

			logEventRequests_ << request;
		}
	}
}

// TODO: QXmlStreamWriter
void YaLogEventsManager::save()
{
	QDomDocument doc;

	QDomElement root = doc.createElement("yalogevents");
	root.setAttribute("version", "1.0");
	doc.appendChild(root);

	DummyStream stream;

	QListIterator<LogEventRequest*> it(logEventRequests_);
	while (it.hasNext()) {
		LogEventRequest* request = it.next();

		QDomElement i = doc.createElement("i");
		i.setAttribute("account", request->accountJid.bare());
		XMLHelper::setBoolAttribute(i, "originLocal", request->originLocal);
		i.setAttribute("timeStamp", request->timeStamp.toYaIsoTime());

		Stanza s = request->message.toStanza(&stream);
		i.appendChild(s.element());

		root.appendChild(i);
	}

	AtomicXmlFile f(fileName());
	if (!f.saveDocument(doc))
		return;
}

void YaLogEventsManager::logEventOnServer(const XMPP::Jid& j, PsiEvent* e, const PsiAccount* account)
{
	Q_UNUSED(j);
	if (e->type() != PsiEvent::Message)
		return;

	MessageEvent* me = static_cast<MessageEvent*>(e);

	YaDateTime timeStamp;
	if (!me->message().yaMessageId().isEmpty())
		timeStamp = XMPP::YaDateTime::fromYaTime_t(me->message().yaMessageId());
	else
		timeStamp = me->timeStamp();

	LogEventRequest* request = new LogEventRequest();
	request->accountJid  = account->jid();
	request->originLocal = me->originLocal();
	request->timeStamp   = timeStamp;
	request->message     = me->message();
	logEventRequests_ << request;

	writeToDiskTimer_->start();
	logEventsOnServer();
}

void YaLogEventsManager::logEventsOnServer()
{
	if (logEventRequests_.isEmpty())
		return;

	PsiAccount* historyAccount = controller_->contactList()->yaServerHistoryAccount();
	if (!historyAccount || !historyAccount->isAvailable()) {
		logEventRequestsTimer_->start();
		return;
	}

	int sentEventsCount = 0;
	QMutableListIterator<LogEventRequest*> it(logEventRequests_);
	while (it.hasNext()) {
		LogEventRequest* request = it.next();
		if (request->task) {
			continue;
		}

		if (request->accountJid == historyAccount->jid()) {
			if (request->task) {
				logEventRequestTasks_.remove(request->task);
			}
			delete request;
			it.remove();
			writeToDiskTimer_->start();
			continue;
		}

		// don't flood the server with our requests
		if (sentEventsCount > 3) {
			logEventRequestsTimer_->start();
			break;
		}

		request->task = new JT_YaLogMessage(historyAccount->client()->rootTask());
		logEventRequestTasks_[request->task] = request;
		connect(request->task, SIGNAL(finished()), SLOT(logMessageFinished()));
		request->task->log(request->originLocal, request->timeStamp, request->accountJid, request->message);
		request->task->go(true);
		++sentEventsCount;
	}
}

void YaLogEventsManager::logMessageFinished()
{
	PsiLogger::instance()->log(QString("YaLogEventsManager::logMessageFinished() %1").arg(logEventRequests_.count()));

	JT_YaLogMessage* task = static_cast<JT_YaLogMessage*>(sender());
	Q_ASSERT(logEventRequestTasks_.contains(task));
	LogEventRequest* request = logEventRequestTasks_[task];
	logEventRequestTasks_.remove(task);

	if (task->success()) {
		logEventRequests_.removeAll(request);
		delete request;
		writeToDiskTimer_->start();
	}
	else {
		request->task = 0;
	}

	logEventsOnServer();
}
