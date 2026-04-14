/*
 * psidbusnotifier.cpp
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

#include "psidbusnotifier.h"

#include <QtDBus>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QImage>
#include <QPixmap>

#include "psiaccount.h"

// copied from https://github.com/romnes/justify/blob/master/src/notify.h
struct iiibiiay
{
	iiibiiay(const QPixmap& img);
	iiibiiay();
	static const int id;
	int width;
	int height;
	int rowstride;
	bool hasAlpha;
	int bitsPerSample;
	int channels;
	QByteArray image;
};

Q_DECLARE_METATYPE(iiibiiay);

QDBusArgument &operator<<(QDBusArgument &a, const iiibiiay &i);
const QDBusArgument & operator >>(const QDBusArgument &a, iiibiiay &i);

QDBusArgument &operator<<(QDBusArgument &a, const iiibiiay &i)
{
	a.beginStructure();
	a << i.width << i.height << i.rowstride << i.hasAlpha << i.bitsPerSample << i.channels << i.image;
	a.endStructure();
	return a;
}

const QDBusArgument & operator >>(const QDBusArgument &a,  iiibiiay &i)
{
	a.beginStructure();
	a >> i.width >> i.height >> i.rowstride >> i.hasAlpha >> i.bitsPerSample >> i.channels >> i.image;
	a.endStructure();
	return a;
}

iiibiiay::iiibiiay(const QPixmap& pixmap)
{
	QImage img = pixmap.toImage();
	if (img.format() != QImage::Format_ARGB32)
		img = img.convertToFormat(QImage::Format_ARGB32);
	width = img.width();
	height = img.height();
	rowstride = img.bytesPerLine();
	hasAlpha = img.hasAlphaChannel();
	channels = img.isGrayscale() ? 1 : hasAlpha ? 4 : 3;
	bitsPerSample = img.depth() / channels;
	image.append((char*)img.rgbSwapped().bits(), img.numBytes());
}

iiibiiay::iiibiiay(){}
const int iiibiiay::id(qDBusRegisterMetaType<iiibiiay>());

PsiDbusNotifier::PsiDbusNotifier(QObject* parent, QString appName)
	: PsiNotifierBase(parent, appName)
{
}

PsiDbusNotifier::~PsiDbusNotifier()
{
}

void PsiDbusNotifier::notify(PsiAccount* account, const XMPP::Jid& jid, const QString& name, const QString& title, const QString& desc, const QPixmap& icon)
{
	Q_UNUSED(name);

	QStringList actions;
	QVariantMap hints;
	iiibiiay dbusIcon(icon);
	hints.insert("icon_data", QVariant(iiibiiay::id, &dbusIcon));

	QDBusInterface notificationInterface("org.freedesktop.Notifications",
	                                     "/org/freedesktop/Notifications",
	                                     "org.freedesktop.Notifications",
	                                     QDBusConnection::sessionBus());
	QVariantList args;
	args << QString(appName());
	args << QVariant(QVariant::UInt);
	args << QString();
	args << title;
	args << desc;
	args << actions;
	args << hints;
	args << -1;
	QDBusMessage recieve = notificationInterface.callWithArgumentList(QDBus::AutoDetect, "Notify", args);

	Q_UNUSED(account);
	Q_UNUSED(jid);
	int id = recieve.arguments().last().toInt();

	NotificationData data;
	data.account = account;
	data.jid = jid;
	notifications_[id] = data;
}
