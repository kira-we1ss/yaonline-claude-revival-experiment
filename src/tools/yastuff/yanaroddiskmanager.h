/*
 * yanaroddiskmanager.h
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

#ifndef YANARODDISKMANAGER_H
#define YANARODDISKMANAGER_H

#include <QObject>
#include <QStringList>
#include <QHash>
#include <QMap>
#include <QDateTime>
#include <QVariantMap>
#include <QRunnable>

#include "httphelper.h"

class QTimer;
class QFile;

class GetFileMd5Runnable : public QObject, public QRunnable
{
	Q_OBJECT
public:
	GetFileMd5Runnable(const QString& fileName, QObject* obj, const char* slot);
	void run();

signals:
	void md5Ready(const QString& fileName, const QString& md5, QObject* obj, const char* slot);

private:
	QString fileName_;
	QPointer<QObject> obj_;
	const char* slot_;
};

class GetBoundaryStringRunnable : public QObject, public QRunnable
{
	Q_OBJECT
public:
	GetBoundaryStringRunnable(QIODevice* data, const QString& id);
	void run();

signals:
	void boundaryStringReady(const QString& boundary, const QString& id);

private:
	QPointer<QIODevice> data_;
	QString id_;
};

class YaNarodDiskManager : public BaseHttpHelper<QObject>
{
	Q_OBJECT
public:
	YaNarodDiskManager(PsiCon* controller);
	~YaNarodDiskManager();

	enum State {
		State_Queued,

		State_GetTokenizedStorageUrl,
		State_GetStorage,
		State_Upload,
		State_UpdatingStatus,

		State_GetTokenizedUidUrl,
		State_GetUid,
		State_SetFlags,

		State_Finished,
		State_Error
	};

	struct RecentFile {
		QString fileName;
		QString url;
		QDateTime dateTime;
		qint64 size;
		QString md5;
		QString uid;
		QString fid;

		bool operator<(const RecentFile& other) const
		{
			return dateTime > other.dateTime;
		}
	};

	void uploadFile(const QString& fileName, QObject* object, const char* slotName);
	void cancelUpload(const QString& id);

	State getState(const QString& id) const;
	QString getFileName(const QString& id) const;
	QString errorString(const QString& id) const;

	void clearRecentFiles();
	QMap<QString, YaNarodDiskManager::RecentFile> recentFiles() const;

	static QString fileMd5(const QString& fileName);
	static QString humanReadableSize(qint64 size);
	static QString humanReadableName(const QString& fileName, qint64 size);

signals:
	void stateChanged(const QString& id, YaNarodDiskManager::State state);
	void uploadProgress(const QString& id, qint64 bytesSent, qint64 bytesTotal);
	void recentFilesChanged();

private slots:
	// reimplemented
	void replyFinished(QNetworkReply* reply);

	void updateQueue();
	void replyUploadProgress(qint64 bytesSent, qint64 bytesTotal);
	void tokenizedUrlFinished(int id, const QString& tokenizedUrl);
	void md5Ready(const QString& fileName, const QString& md5, QObject* obj, const char* slot);
	void boundaryStringReady(const QString& boundary, const QString& id);

private:
	struct Request {
		Request()
			: reply(0)
		{}

		Request(const QString& _id)
			: id(_id)
			, state(State_Error)
			, tokenizedStorageRequestId(0)
			, reply(0)
		{}

		QString id; // is md5
		State state;
		QString errorString;

		int tokenizedStorageRequestId;
		QString uploadUrl;
		QString uploadHash;
		QString uploadProgressUrl;
		QVariantMap fileMap;
		QString uid;

		QString fileName;
		QPointer<QFile> ioDevice;
		QNetworkReply* reply;

		QStringList uploadedUrls;
	};

	void processStorageReply(Request* r, QNetworkReply* reply);
	void startUpload(Request* r);
	void startStatusUpdate(Request* r, QNetworkReply* reply);
	void updatingStatus(Request* r, QNetworkReply* reply);
	void gotUid(Request* r, QNetworkReply* reply);
	void setFlags(Request* r, QNetworkReply* reply);
	QString addRecentFile(const QString& fileName, const QString& md5, const QString& uid, const QVariantMap& fileMap);

	void saveRecentFiles();
	void loadRecentFiles();

	QTimer* updateQueueTimer_;
	QMap<QString, Request> requests_;
	QMap<QString, RecentFile> recentFiles_;
};

#endif
