/*
 * httphelper.cpp
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

#include "httphelper.h"

#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUuid>
#include <QTimer>
#include <QNetworkProxy>
#include <QBuffer>

#define USE_APPLICATIONINFO

#ifdef USE_APPLICATIONINFO
#include "applicationinfo.h"
#endif

#include "textutil.h"
#include "psicon.h"
#include "proxy.h"
#include "psicontactlist.h"
#include "psiaccount.h"
#include "fileuploaddevice.h"

//----------------------------------------------------------------------------
// GHttpHelper
//----------------------------------------------------------------------------

GHttpHelper::GHttpHelper(PsiCon* controller, QObject* parent)
	: QObject(parent)
	, controller_(controller)
	, network_(0)
{
	Q_ASSERT(controller_);
	Q_ASSERT(parent);

	QTimer::singleShot(0, this, SLOT(init()));
}

void GHttpHelper::init()
{
	if (network_) {
		return;
	}

	network_ = new QNetworkAccessManager(this);
	connect(network_, SIGNAL(finished(QNetworkReply*)), parent(), SLOT(replyFinished(QNetworkReply*)));
	Q_ASSERT(parent()->metaObject()->indexOfSlot("replyFinished(QNetworkReply*)") != -1);

	if (controller_) {
		connect(controller_->proxy(), SIGNAL(settingsChanged()), SLOT(updateProxy()), Qt::QueuedConnection);
		updateProxy();
	}
}

QNetworkAccessManager* GHttpHelper::network()
{
	return network_;
}

void GHttpHelper::updateProxy()
{
	QNetworkProxy proxy;
	PsiAccount* account = !controller_->contactList()->accounts().isEmpty() ?
	                      controller_->contactList()->accounts().first() :
	                      0;
	if (account && account->userAccount().proxy_index) {
		ProxyItem p = controller_->proxy()->getItem(account->userAccount().proxy_index-1);
		QNetworkProxy::ProxyType type = QNetworkProxy::NoProxy;
		if (p.type == "http" || p.type == "poll")
			type = QNetworkProxy::HttpProxy;
		else if (p.type == "socks")
			type = QNetworkProxy::Socks5Proxy;

		proxy = QNetworkProxy(type, p.settings.host, p.settings.port, p.settings.user, p.settings.pass);
	}

	network_->setProxy(proxy);
}

//----------------------------------------------------------------------------
// HttpHelper
//----------------------------------------------------------------------------

namespace HttpHelper {

struct ProcessedUrl {
	ProcessedUrl(const QUrl& _url, const QString& _fullUri)
		: url(_url)
		, fullUri(_fullUri)
	{}

	QUrl url;
	QString fullUri;
};

static ProcessedUrl processUrl(const QString& urlString)
{
	QUrl url(urlString, QUrl::TolerantMode);
	// Q_ASSERT(url.hasQuery());

	QString query = url.query();
	query.replace("?", "&"); // FIXME: Bug in Qt 4.4.2?

	QString fullUri = url.path();
	if (!query.isEmpty()) {
		fullUri += "?" + query;
	}

	return ProcessedUrl(url, fullUri);
}

static QString userAgent()
{
#ifdef YAPSI
	return QString("Ya.Online %1").arg(ApplicationInfo::version());
#elif defined(USE_APPLICATIONINFO)
	return QString("%1 %2")
	       .arg(ApplicationInfo::name())
	       .arg(ApplicationInfo::version());
#else
	return "QHttp";
#endif
}


QNetworkRequest getRequest(const QString& urlString, QNetworkReply* referer)
{
	ProcessedUrl url = processUrl(urlString);

	QNetworkRequest result(url.url);
	result.setRawHeader("User-Agent", userAgent().toUtf8());

	if (referer) {
		result.setRawHeader("Referer", referer->url().toEncoded());
	}

	return result;
}

QString getBoundaryString(QIODevice* data)
{
	qint64 pos = data->pos();

	QString uuid;
	while (true) {
		uuid = QUuid::createUuid().toString().replace(QRegExp("\\{|\\-|\\}"), "");
		QByteArray uuidUtf8 = uuid.toUtf8();

		bool found = false;;

		QByteArray buffer(1024 * 1024, 0);
		data->seek(0);

		int len;
		while ((len = data->read(buffer.data(), buffer.size())) > 0) {
			buffer.resize(len);
			if (buffer.contains(uuidUtf8)) {
				found = true;
				break;
			}
		}

		if (!found)
			break;
	}

	data->seek(pos);
	return uuid;
}

QNetworkRequest postFileRequest(const QString& urlString, const QString& fieldName, const QString& fileName, FileUploadDevice* postData)
{
	QString boundary = postData->boundaryString();

	postData->preData()->append("--" + boundary + "\r\n");
	postData->preData()->append("Content-Disposition: form-data; name=\"" + TextUtil::escape(fieldName) +
	                            "\"; filename=\"" + TextUtil::escape(fileName.toUtf8()) + "\"\r\n");
	postData->preData()->append("Content-Type: application/octet-stream\r\n");
	postData->preData()->append("\r\n");
	// postData->append(fileData);
	postData->postData()->append("\r\n--" + boundary + "--\r\n");

	ProcessedUrl url = processUrl(urlString);

	QNetworkRequest result(url.url);
	result.setRawHeader("User-Agent", userAgent().toUtf8());
	result.setRawHeader("Content-Type", "multipart/form-data, boundary=" + boundary.toLatin1());
	result.setRawHeader("Content-Length", QString::number(postData->size()).toUtf8());
	return result;
}

QNetworkRequest postFileRequest(const QString& urlString, const QString& fieldName, const QString& fileName, const QByteArray& fileData, QByteArray* postData)
{
	QByteArray randomAccess = QByteArray::fromRawData(fileData.constData(), fileData.size());
	QBuffer buffer(&randomAccess);
	buffer.open(QIODevice::ReadOnly);
	QString boundary = getBoundaryString(&buffer);

	postData->append("--" + boundary + "\r\n");
	postData->append("Content-Disposition: form-data; name=\"" + TextUtil::escape(fieldName) +
	                "\"; filename=\"" + TextUtil::escape(fileName.toUtf8()) + "\"\r\n");
	postData->append("Content-Type: application/octet-stream\r\n");
	postData->append("\r\n");
	postData->append(fileData);
	postData->append("\r\n--" + boundary + "--\r\n");

	ProcessedUrl url = processUrl(urlString);

	QNetworkRequest result(url.url);
	result.setRawHeader("User-Agent", userAgent().toUtf8());
	result.setRawHeader("Content-Type", "multipart/form-data, boundary=" + boundary.toLatin1());
	result.setRawHeader("Content-Length", QString::number(postData->size()).toUtf8());
	return result;
}

QNetworkReply* needRedirect(QNetworkAccessManager* network, QNetworkReply* reply, const QByteArray& data)
{
	static QRegExp windowLocationReplaceRx("window\\.location\\.replace\\(\\\"(.+)\\\"\\);");
	windowLocationReplaceRx.setMinimal(true);

	// HttpHelper::debugReply(reply);

	QString body = QString::fromUtf8(data);
	if (reply->hasRawHeader("Location")) {
		QString url = HttpHelper::urlDecode(QString::fromUtf8(reply->rawHeader("Location")));
		return network->get(HttpHelper::getRequest(url));
	}
	else if (windowLocationReplaceRx.indexIn(body) != -1) {
		QString url = HttpHelper::urlDecode(windowLocationReplaceRx.capturedTexts().last());
		return network->get(HttpHelper::getRequest(url));
	}

	return 0;
}

QString urlEncode(const QString& str)
{
	return QString::fromUtf8(QUrl::toPercentEncoding(str.toUtf8()));
}

QString urlDecode(const QString& str)
{
	return QUrl::fromPercentEncoding(str.toUtf8());
}

void debugReply(QNetworkReply* reply)
{
	qWarning("*** %s / %s", reply->url().toEncoded().constData(), qPrintable(reply->errorString()));

	foreach(const QByteArray& header, reply->request().rawHeaderList()) {
		qWarning("\t%s: %s", header.constData(), reply->request().rawHeader(header).constData());
	}

	qWarning(" ");

	foreach(const QByteArray& header, reply->rawHeaderList()) {
		qWarning("\t%s: %s", header.constData(), reply->rawHeader(header).constData());
	}
	qWarning("***\n");
}

}; // namespace HttpHelper
