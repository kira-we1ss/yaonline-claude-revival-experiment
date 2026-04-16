/*
 * psilogger.cpp - a simple logger class
 * Copyright (C) 2008  Yandex LLC (Michail Pishchagin)
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

#include "psilogger.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QSettings>
#include <QDir>

#include "applicationinfo.h"
#ifdef YAPSI
#include "yadayuse.h"
#endif

PsiLogger* PsiLogger::instance_ = 0;
static bool quitting_application = false;

// TODO: Look at Q_FUNC_INFO macro as a possible improvement to LOG_TRACE

PsiLogger::PsiLogger()
	: QObject(QCoreApplication::instance())
	, file_(0)
	, stream_(0)
{
	bool enableLogging = isLoggingEnabled();

	{
		char* p = getenv("ENABLE_LOGGING");
		if (p) {
			enableLogging = true;
		}
	}

	if (!enableLogging)
		return;

	QString fileName = logFileName();
	QFile::remove(fileName);
	file_ = new QFile(fileName);
	if (!file_->open(QIODevice::WriteOnly)) {
		qWarning("unable to open log file");
	}
	
	stream_ = new QTextStream();
	stream_->setDevice(file_);
	stream_->setCodec("UTF-8");
#ifdef YAPSI
	log(QString("*** LOG STARTED %1 (%2 / %3) %4")
	    .arg(YaDayUse::ver())
	    .arg(YaDayUse::osId())
	    .arg(YaDayUse::osVer())
	    .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
#else
	log(QString("*** LOG STARTED %1")
	    .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
#endif
}

PsiLogger::~PsiLogger()
{
	if (instance_ == this) {
		instance_ = 0;
		quitting_application = true;

		delete stream_;
		delete file_;
	}
}

PsiLogger* PsiLogger::instance()
{
	if (!instance_ && !quitting_application)
		instance_ = new PsiLogger();
	return instance_;
}

bool PsiLogger::enableLogging()
{
	return this && stream_;
}

void PsiLogger::log(const QString& _msg)
{
	if (!enableLogging())
		return;

	QDateTime time = QDateTime::currentDateTime();
	QString msg = QString().sprintf("%02d:%02d:%02d ", time.time().hour(), time.time().minute(), time.time().second());
	msg += _msg;

	*stream_ << msg << "\n";
	stream_->flush();
}

void PsiLogger::trace(const char* file, int line, const char* func_info)
{
	log(QString("%1:%2 (%3)").arg(file).arg(line).arg(func_info));
}

QString PsiLogger::logFileName()
{
#ifdef YAPSI
	QString fileName = ApplicationInfo::homeDir() + "/";
	fileName += "yachat-log.txt";
#else
	QString fileName = QDir::homePath() + "/";
	fileName += "psilogger.txt";
#endif
	return fileName;
}

static const QString extraLogKey = "extra_log";
static const QString extraLogMessagesKey = "extra_log_messages";

static bool isOptionEnabled(const QString& keyName, const QString& fileName)
{
	bool enableLogging = false;
#ifdef Q_OS_WIN
	Q_UNUSED(fileName);
	QSettings sUser(QSettings::UserScope, "Yandex", "Online");
	enableLogging = sUser.contains(keyName);
#else
	Q_UNUSED(keyName);
	if (QFile::exists(fileName))
		enableLogging = true;
#endif
	return enableLogging;
}

static void setOptionEnabled(bool enable, const QString& keyName, const QString& fileName)
{
#ifdef Q_OS_WIN
	Q_UNUSED(fileName);
	QSettings sUser(QSettings::UserScope, "Yandex", "Online");
	if (enable)
		sUser.setValue(keyName, QString());
	else
		sUser.remove(keyName);
#else
	Q_UNUSED(keyName);
	if (enable) {
		QFile file(fileName);
		file.open(QIODevice::WriteOnly);
	}
	else {
		QFile::remove(fileName);
	}
#endif
}

bool PsiLogger::isLoggingEnabled()
{
	return isOptionEnabled(extraLogKey,
	                       ApplicationInfo::homeDir() + "/" + extraLogKey);
}

void PsiLogger::setLoggingEnabled(bool enable)
{
	setOptionEnabled(enable,
	                 extraLogKey,
	                 ApplicationInfo::homeDir() + "/" + extraLogKey);
}

bool PsiLogger::isLogAllMessagesEnabled()
{
	return isOptionEnabled(extraLogMessagesKey,
	                       ApplicationInfo::homeDir() + "/" + extraLogMessagesKey);
}

void PsiLogger::setLogAllMessagesEnabled(bool enable)
{
	setOptionEnabled(enable,
	                 extraLogMessagesKey,
	                 ApplicationInfo::homeDir() + "/" + extraLogMessagesKey);
}
