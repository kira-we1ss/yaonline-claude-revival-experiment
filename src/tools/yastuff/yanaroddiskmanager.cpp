/*
 * yanaroddiskmanager.cpp
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

#include "yanaroddiskmanager.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMutableListIterator>
#include <QRegExp>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDomDocument>
#include <QTimer>

#include <qca.h>

#include "yatokenauth.h"
#include "httphelper.h"
#include "JsonToVariant.h"
#include "psioptions.h"
#include "xmpp_xmlcommon.h"
#include "psicon.h"
#include "fileuploaddevice.h"
#include "psilogger.h"

static const QString recentFilesOptionPath = "options.ui.file-transfer.recent-files";

// #define ENABLE_RECENT_FILES

//----------------------------------------------------------------------------
// GetFileMd5Runnable
//----------------------------------------------------------------------------

GetFileMd5Runnable::GetFileMd5Runnable(const QString& fileName, QObject* obj, const char* slot)
	: QObject(), QRunnable()
	, fileName_(fileName)
	, obj_(obj)
	, slot_(slot)
{
}

void GetFileMd5Runnable::run()
{
	QString md5 = YaNarodDiskManager::fileMd5(fileName_);
	emit md5Ready(fileName_, md5, obj_, slot_);
}

//----------------------------------------------------------------------------
// GetBoundaryStringRunnable
//----------------------------------------------------------------------------

GetBoundaryStringRunnable::GetBoundaryStringRunnable(QIODevice* data, const QString& id)
	: QObject(), QRunnable()
	, data_(data)
	, id_(id)
{
}

void GetBoundaryStringRunnable::run()
{
	QString boundary = HttpHelper::getBoundaryString(data_);
	emit boundaryStringReady(boundary, id_);
}

//----------------------------------------------------------------------------
// YaNarodDiskManager
//----------------------------------------------------------------------------

YaNarodDiskManager::YaNarodDiskManager(PsiCon* controller)
	: BaseHttpHelper<QObject>(controller, controller)
{
	connect(YaTokenAuth::instance(), SIGNAL(finished(int, const QString&)), SLOT(tokenizedUrlFinished(int, const QString&)));

	updateQueueTimer_ = new QTimer(this);
	updateQueueTimer_->setSingleShot(true);
	updateQueueTimer_->setInterval(100);
	connect(updateQueueTimer_, SIGNAL(timeout()), SLOT(updateQueue()));

#ifdef ENABLE_RECENT_FILES
	loadRecentFiles();
#endif
}

YaNarodDiskManager::~YaNarodDiskManager()
{
	saveRecentFiles();
}

void YaNarodDiskManager::uploadFile(const QString& fileName, QObject* object, const char* slotName)
{
	GetFileMd5Runnable* runnable = new GetFileMd5Runnable(fileName, object, slotName);
	connect(runnable, SIGNAL(md5Ready(const QString&, const QString&, QObject*, const char*)),
	                    SLOT(md5Ready(const QString&, const QString&, QObject*, const char*)));
	QThreadPool::globalInstance()->start(runnable);
}

void YaNarodDiskManager::md5Ready(const QString& fileName, const QString& md5, QObject* obj, const char* slot)
{
	if (md5.isEmpty()) {
		return;
	}

	// empty file md5
	if (md5 == "d41d8cd98f00b204e9800998ecf8427e") {
		return;
	}

	QString result;
#ifdef ENABLE_RECENT_FILES
	if (recentFiles_.contains(md5)) {
		result = md5;
		return;
	}
#endif

	if (requests_.contains(md5) && requests_[md5].state == State_Error) {
		cancelUpload(md5);
	}

	if (result.isEmpty() && requests_.contains(md5)) {
		result = md5;
		return;
	}

	if (result.isEmpty()) {
		Request request(md5);
		request.fileName = fileName;
		request.state = State_Queued;

		updateQueueTimer_->start();
		requests_[md5] = request;

		result = request.id;
	}

	QMetaObject::invokeMethod(obj, slot, Qt::DirectConnection,
	                           QGenericReturnArgument(),
	                           Q_ARG(QString, result));
}

void YaNarodDiskManager::cancelUpload(const QString& id)
{
	if (requests_.contains(id)) {
		Request r = requests_[id];
		if (r.reply) {
			r.reply->abort();
			r.reply->deleteLater();
		}
		requests_.remove(id);
		updateQueueTimer_->start();
	}
}

void YaNarodDiskManager::updateQueue()
{
	bool hasWorking = false;
	QMutableMapIterator<QString, Request> it(requests_);
	while (it.hasNext()) {
		it.next();
		Request r = it.value();
		if (r.state != State_Queued &&
		    r.state != State_Finished &&
		    r.state != State_Error)
		{
			hasWorking = true;
			break;
		}
	}

	if (hasWorking)
		return;

	it.toFront();
	while (it.hasNext()) {
		it.next();
		Request r = it.value();
		if (r.state == State_Queued) {
			r.tokenizedStorageRequestId = YaTokenAuth::instance()->getTokenizedUrl("http://narod.yandex.ru/disk/getstorage");
			if (r.tokenizedStorageRequestId == -1) {
				PsiLogger::instance()->log(QString("YaNarodDiskManager::updateQueue() - authentication error"));
				r.state = State_Error;
				r.errorString = tr("Authentication error");
			}
			else {
				r.state = State_GetTokenizedStorageUrl;
			}

			it.setValue(r);
			break;
		}
	}
}

YaNarodDiskManager::State YaNarodDiskManager::getState(const QString& id) const
{
#ifdef ENABLE_RECENT_FILES
	if (recentFiles_.contains(id)) {
		return State_Finished;
	}
#endif

	if (requests_.contains(id)) {
		return requests_[id].state;
	}

	return State_Error;
}

QString YaNarodDiskManager::getFileName(const QString& id) const
{
#ifdef ENABLE_RECENT_FILES
	if (recentFiles_.contains(id)) {
		return recentFiles_[id].fileName;
	}
#endif

	if (requests_.contains(id)) {
		return requests_[id].fileName;
	}

	return QString();
}

QString YaNarodDiskManager::errorString(const QString& id) const
{
#ifdef ENABLE_RECENT_FILES
	if (recentFiles_.contains(id)) {
		return QString();
	}
#endif

	if (requests_.contains(id)) {
		return requests_[id].errorString;
	}

	return QString();
}

void YaNarodDiskManager::replyFinished(QNetworkReply* reply)
{
	reply->deleteLater();

	QMutableMapIterator<QString, Request> it(requests_);
	while (it.hasNext()) {
		it.next();
		Request r = it.value();
		if (r.reply == reply) {
			// qWarning("YaNarodDiskManager::replyFinished");
			r.reply = 0;

			if (r.state == State_GetStorage) {
				processStorageReply(&r, reply);
			}
			else if (r.state == State_Upload) {
				startStatusUpdate(&r, reply);
			}
			else if (r.state == State_UpdatingStatus) {
				updatingStatus(&r, reply);
			}
			else if (r.state == State_GetUid) {
				gotUid(&r, reply);
			}
			else if (r.state == State_SetFlags) {
				setFlags(&r, reply);
			}

			it.setValue(r);
			emit stateChanged(r.id, r.state);
			updateQueueTimer_->start();

			if (r.state == State_Finished) {
				it.remove();
			}
			break;
		}
	}
}

void YaNarodDiskManager::replyUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
	QNetworkReply* reply = static_cast<QNetworkReply*>(sender());
	QMutableMapIterator<QString, Request> it(requests_);
	while (it.hasNext()) {
		it.next();
		Request r = it.value();
		if (r.reply == reply) {
			emit uploadProgress(r.id, bytesSent, bytesTotal);
			break;
		}
	}
}

void YaNarodDiskManager::processStorageReply(YaNarodDiskManager::Request* r, QNetworkReply* reply)
{
	QByteArray data = reply->readAll();
	QString body = QString::fromUtf8(data);

	QNetworkReply* redirect = HttpHelper::needRedirect(network(), reply, data);
	if (redirect) {
		r->reply = redirect;
	}
	else {
		r->state = State_Error;
		r->errorString = tr("Error processing storage reply");

		static QRegExp storageRx("getStorage\\((.+)\\);");
		if (storageRx.indexIn(body) != -1) {
			QString jsonStr = storageRx.capturedTexts().last();

			QVariant variant;
			try {
				variant = JsonQt::JsonToVariant::parse(jsonStr);
			}
			catch(...) {
			}
			QVariantMap map = variant.toMap();

			r->uploadUrl = map["url"].toString();
			r->uploadHash = map["hash"].toString();
			r->uploadProgressUrl = map["purl"].toString();
			if (!r->uploadUrl.isEmpty() && !r->uploadHash.isEmpty() && !r->uploadProgressUrl.isEmpty()) {
				startUpload(r);
			}
		}
	}
}

void YaNarodDiskManager::startUpload(Request* r)
{
	QFile* file = new QFile(r->fileName);
	if (!file->open(QIODevice::ReadOnly)) {
		PsiLogger::instance()->log(QString("YaNarodDiskManager::startUpload() - unable to open file %s")
		                           .arg(r->fileName));
		delete file;
		return;
	}

	r->state = State_Upload;
	r->ioDevice = file;
	GetBoundaryStringRunnable* runnable = new GetBoundaryStringRunnable(r->ioDevice, r->id);
	connect(runnable, SIGNAL(boundaryStringReady(const QString&, const QString&)),
	                    SLOT(boundaryStringReady(const QString&, const QString&)));
	QThreadPool::globalInstance()->start(runnable);
}

void YaNarodDiskManager::boundaryStringReady(const QString& boundary, const QString& id)
{
	Q_ASSERT(requests_.contains(id));
	if (!requests_.contains(id))
		return;
	Request r = requests_[id];

	FileUploadDevice* postData = new FileUploadDevice(r.ioDevice, boundary);
	QFileInfo fileInfo(*r.ioDevice);

	QString postUrl = r.uploadUrl + "?tid=" + r.uploadHash;
	QNetworkRequest request = HttpHelper::postFileRequest(postUrl, "file", fileInfo.fileName(), postData);

	postData->open(QIODevice::ReadOnly);

	r.reply = network()->post(request, postData);
	postData->setParent(r.reply);

	connect(r.reply, SIGNAL(uploadProgress(qint64, qint64)), SLOT(replyUploadProgress(qint64, qint64)));
	requests_[id] = r;
}

void YaNarodDiskManager::startStatusUpdate(Request* r, QNetworkReply* reply)
{
	if (reply->error() != QNetworkReply::NoError) {
		PsiLogger::instance()->log(QString("YaNarodDiskManager::startStatusUpdate(%1) - error")
		                           .arg(reply->url().toString()));
		r->state = State_Error;
		r->errorString = tr("Error processing start status reply");
		return;
	}

	r->state = State_UpdatingStatus;
	QString statusUrl = r->uploadProgressUrl + "?tid=" + r->uploadHash;
	r->reply = network()->get(HttpHelper::getRequest(statusUrl));
}

void YaNarodDiskManager::updatingStatus(Request* r, QNetworkReply* reply)
{
	if (reply->error() != QNetworkReply::NoError) {
		PsiLogger::instance()->log(QString("YaNarodDiskManager::updatingStatus(%1) - error")
		                           .arg(reply->url().toString()));
		r->state = State_Error;
		r->errorString = tr("Error processing status update reply");
		return;
	}

	QByteArray data = reply->readAll();
	QString body = QString::fromUtf8(data);

	bool done = false;

	static QRegExp progressRx("getProgress\\((.+)\\);");
	if (progressRx.indexIn(body) != -1) {
		QString jsonStr = progressRx.capturedTexts().last();

		QVariant variant;
		try {
			variant = JsonQt::JsonToVariant::parse(jsonStr);
		}
		catch(...) {
		}
		QVariantMap map = variant.toMap();

		QString status = map["status"].toString();
		if (status == "done") {
			done = true;
			QVariantList files = map["files"].toList();
			foreach(const QVariant& file, files) {
				r->fileMap = file.toMap();
			}

			// r->state = State_GetTokenizedUidUrl;
			// r->tokenizedStorageRequestId = YaTokenAuth::instance()->getTokenizedUrl("http://mail.yandex.ru/api/compose_check");

			r->state = State_GetUid;
			r->reply = network()->get(HttpHelper::getRequest("http://mail.yandex.ru/api/compose_check"));
		}
	}

	if (!done) {
		startStatusUpdate(r, reply);
	}
}

void YaNarodDiskManager::gotUid(Request* r, QNetworkReply* reply)
{
	bool success = reply->error() == QNetworkReply::NoError;

	QByteArray data = success ? reply->readAll() : QByteArray();
	QNetworkReply* redirect = success? HttpHelper::needRedirect(network(), reply, data) : 0;

	if (!success) {
		// pass
	}
	else if (redirect) {
		r->reply = redirect;
	}
	else {
		success = false;
		QString body = QString::fromUtf8(data);

		QDomDocument doc;
		if (doc.setContent(data)) {
			QDomElement root = doc.documentElement();
			if (root.tagName() == "yamail") {
				bool found;
				QDomElement account_information = findSubTag(root, "account_information", &found);
				if (found) {
					QString uid = XMLHelper::subTagText(account_information, "uid");
					if (!uid.isEmpty()) {
						r->uid = uid;
						success = true;

						// remove uid ?
						r->state = State_SetFlags;
						QString flagsUrl = QString("http://narod.yandex.ru/disk/internal/setflag/?uid=%1&fid=%2&flag=42")
						                   .arg(r->uid)
						                   .arg(r->fileMap["fid"].toString());
						r->reply = network()->get(HttpHelper::getRequest(flagsUrl));
					}
				}
			}
		}
	}

	if (!success) {
		PsiLogger::instance()->log(QString("YaNarodDiskManager::gotUid(%1) - error")
		                           .arg(reply->url().toString()));
		r->state = State_Error;
		r->errorString = tr("Error processing uid reply");
		return;
	}
}

void YaNarodDiskManager::setFlags(Request* r, QNetworkReply* reply)
{
	if (reply->error() != QNetworkReply::NoError) {
		PsiLogger::instance()->log(QString("YaNarodDiskManager::setFlags(%1) - error")
		                           .arg(reply->url().toString()));
		r->state = State_Error;
		r->errorString = tr("Error processing set flags reply");
		return;
	}

	QByteArray data = reply->readAll();
	QString dataString(data);
	if (dataString != "ok") {
		qWarning("YaNarodDiskManager::setFlags(): '%s' returned", qPrintable(dataString));
	}

	QString fileUrl = addRecentFile(r->fileName, r->id, r->uid, r->fileMap);
	r->uploadedUrls << fileUrl;
	r->state = State_Finished;
}

void YaNarodDiskManager::tokenizedUrlFinished(int id, const QString& tokenizedUrl)
{
	QMutableMapIterator<QString, Request> it(requests_);
	while (it.hasNext()) {
		it.next();
		Request r = it.value();
		if (r.tokenizedStorageRequestId == id) {
			r.tokenizedStorageRequestId = -1;
			if (tokenizedUrl.isEmpty()) {
				PsiLogger::instance()->log(QString("YaNarodDiskManager::tokenizedUrlFinished - error"));
				r.state = State_Error;
				r.errorString = tr("Authentication error");
			}
			else {
				if (r.state == State_GetTokenizedUidUrl) {
					r.state = State_GetUid;
					r.reply = network()->get(HttpHelper::getRequest(tokenizedUrl));
				}
				else {
					Q_ASSERT(r.state == State_GetTokenizedStorageUrl);
					r.state = State_GetStorage;
					r.reply = network()->get(HttpHelper::getRequest(tokenizedUrl));
				}
			}

			it.setValue(r);
			emit stateChanged(r.id, r.state);
			updateQueueTimer_->start();
			break;
		}
	}
}

QString YaNarodDiskManager::fileMd5(const QString& fileName)
{
	if (!QFile::exists(fileName))
		return QString();

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
	         return QString();

	QCA::Hash hashObj("md5");
	hashObj.update(&file);
	QByteArray result = hashObj.final().toByteArray();
	return QCA::arrayToHex(result);
}

QString YaNarodDiskManager::humanReadableSize(qint64 size)
{
	static QStringList units;
	if (units.isEmpty()) {
		units = tr("B;KB;MB;GB;TB;PB").split(";");
	}
	int base = 1024;

	int i = 0;
	for (; size >= base; ++i) {
		if (i == units.count()) {
			--i;
			break;
		}

		size /= base;
	}

	return QString("%1 %2")
	       .arg(size)
	       .arg(units[i]);
}

QString YaNarodDiskManager::humanReadableName(const QString& fileName, qint64 size)
{
	QFileInfo fi(fileName);

	if (!size) {
		return fi.fileName();
	}

	return QString("%1 (%2)")
	       .arg(fi.fileName())
	       .arg(humanReadableSize(size));
}

QString YaNarodDiskManager::addRecentFile(const QString& fileName, const QString& md5, const QString& uid, const QVariantMap& fileMap)
{
	QString fileUrl = QString("http://narod.ru/disk/%1/%2")
	                  .arg(fileMap["hash"].toString())
	                  .arg(fileMap["name"].toString());

	QFileInfo fi(fileName);
	if (recentFiles_.contains(md5)) {
		recentFiles_.remove(md5);
	}

	RecentFile f;
	f.fileName = fileName;
	f.url = fileUrl;
	f.dateTime = QDateTime::currentDateTime();
	f.size = fi.size();
	f.md5 = md5;
	f.uid = uid;
	f.fid = fileMap["fid"].toString();
	recentFiles_[md5] = f;

	saveRecentFiles();
	emit recentFilesChanged();
	return fileUrl;
}

void YaNarodDiskManager::saveRecentFiles()
{
	{
		QList<RecentFile> files;
		QMapIterator<QString, YaNarodDiskManager::RecentFile> it(recentFiles_);
		while (it.hasNext()) {
			it.next();
			files << it.value();
		}

		qSort(files);

		while (files.count() > 100) {
			RecentFile f = files.takeLast();
			recentFiles_.remove(f.md5);
		}
	}

	QDomDocument doc;
	QDomElement root = doc.createElement("items");
	root.setAttribute("version", "1.0");
	doc.appendChild(root);

	QMapIterator<QString, RecentFile> it(recentFiles_);
	while (it.hasNext()) {
		it.next();

		QDomElement tag = textTag(&doc, "file", it.value().url);
		tag.setAttribute("name", it.value().fileName);
		tag.setAttribute("dateTime", it.value().dateTime.toString(Qt::ISODate));
		tag.setAttribute("size", it.value().size);
		tag.setAttribute("md5", it.key());
		tag.setAttribute("uid", it.value().uid);
		tag.setAttribute("fid", it.value().fid);
		root.appendChild(tag);
	}

	PsiOptions::instance()->setOption(recentFilesOptionPath, doc.toString());
}

void YaNarodDiskManager::loadRecentFiles()
{
	QDomDocument doc;
	if (!doc.setContent(PsiOptions::instance()->getOption(recentFilesOptionPath).toString()))
		return;

	QDomElement root = doc.documentElement();
	if (root.tagName() != "items" || root.attribute("version") != "1.0")
		return;

	recentFiles_.clear();
	for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement e = n.toElement();
		if (e.isNull())
			continue;

		if (e.tagName() == "file") {
			RecentFile f;
			f.url = e.text();
			f.fileName = e.attribute("name");
			f.dateTime = QDateTime::fromString(e.attribute("dateTime"), Qt::ISODate);
			f.size = e.attribute("size").toULongLong();
			f.md5 = e.attribute("md5");
			f.uid = e.attribute("uid");
			f.fid = e.attribute("fid");

			if (f.url.isEmpty() ||
			    f.fileName.isEmpty() ||
			    f.dateTime.isNull() ||
			    f.md5.isEmpty() ||
			    f.uid.isEmpty() ||
			    f.fid.isEmpty())
			{
				continue;
			}

			recentFiles_[f.md5] = f;
		}
	}
}

void YaNarodDiskManager::clearRecentFiles()
{
	recentFiles_.clear();
	saveRecentFiles();
	emit recentFilesChanged();
}

QMap<QString, YaNarodDiskManager::RecentFile> YaNarodDiskManager::recentFiles() const
{
	return recentFiles_;
}
