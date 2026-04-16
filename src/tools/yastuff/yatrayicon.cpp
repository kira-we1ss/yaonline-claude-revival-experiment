/*
 * yatrayicon.cpp - custom tray icon
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

#include "yatrayicon.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPointer>
#include <QTimer>

#include "globaleventqueue.h"
#include "psicon.h"
#include "psievent.h"
#include "psiiconset.h"
#include "yacommon.h"
#include "yavisualutil.h"

class YaTrayIconDestroyer : public QObject
{
	Q_OBJECT
public:
	YaTrayIconDestroyer(YaTrayIcon* trayIcon)
		: QObject(QCoreApplication::instance())
		, trayIcon_(trayIcon)
	{
		connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), SLOT(terminating()));
	}

private slots:
	void terminating()
	{
		if (!trayIcon_.isNull())
			delete trayIcon_;
	}

private:
	QPointer<YaTrayIcon> trayIcon_;
};

static QPixmap getTrayPixmap(XMPP::Status::Type statusType, bool haveConnectingAccounts)
{
	static QHash<QString, QPixmap> trayPixmaps;
	QString fileName;
	switch (statusType) {
	case XMPP::Status::Offline:
		fileName = ":images/trayicon/offline.png";
		break;
	case XMPP::Status::Away:
	case XMPP::Status::XA:
		fileName = haveConnectingAccounts ?
		           ":images/trayicon/problem_away.png" :
		           ":images/trayicon/away.png";
		break;
	case XMPP::Status::DND:
		fileName = haveConnectingAccounts ?
		           ":images/trayicon/problem_dnd.png" :
		           ":images/trayicon/dnd.png";
		break;
	case XMPP::Status::Online:
	default:
		fileName = haveConnectingAccounts ?
		           ":images/trayicon/problem.png" :
		           ":images/trayicon/online.png";
		break;
	}

	if (!trayPixmaps.contains(fileName)) {
		trayPixmaps[fileName] = QPixmap(fileName);
	}

	return trayPixmaps[fileName];
}

YaTrayIcon::YaTrayIcon()
	: QSystemTrayIcon(QCoreApplication::instance())
	, psi_(0)
	, haveConnectingAccounts_(false)
{
	new YaTrayIconDestroyer(this);
	connect(this, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(tray_activated(QSystemTrayIcon::ActivationReason)));
	connect(GlobalEventQueue::instance(), SIGNAL(queueChanged()), SLOT(update()));

	updateTimer_ = new QTimer(this);
	updateTimer_->setSingleShot(false);
	// we have to periodically update our icon so the icon would
	// not be considered inactive and hidden by Windows
	updateTimer_->setInterval(1000);
	connect(updateTimer_, SIGNAL(timeout()), SLOT(updateIcon()));
#ifdef Q_OS_WIN
	if (QSysInfo::WindowsVersion < QSysInfo::WV_WINDOWS7) {
		updateTimer_->start();
	}
#endif

#ifndef Q_OS_MAC
	setToolTip(tr("Ya.Online"));
#endif
}

YaTrayIcon::~YaTrayIcon()
{
}

void YaTrayIcon::setPsi(PsiCon* psi)
{
	psi_ = psi;
	if (psi_) {
		connect(psi_, SIGNAL(accountActivityChanged()), SLOT(updateIcon()));
	}
}

YaTrayIcon* YaTrayIcon::instance(PsiCon* psi)
{
	if (!instance_) {
		instance_ = new YaTrayIcon();
	}
	instance_->setPsi(psi);
	return instance_;
}

void YaTrayIcon::tray_activated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::Trigger)
		emit clicked(); // Qt::LeftButton
	else if (reason == QSystemTrayIcon::MiddleClick)
		emit clicked(); // Qt::MidButton
	else if (reason == QSystemTrayIcon::DoubleClick)
		emit doubleClicked();
}

void YaTrayIcon::updateIcon()
{
	QPixmap pix;
	if (!GlobalEventQueue::instance()->ids().isEmpty()) {
		pix = QPixmap(":images/chat_windowicon.png");
	}
	else {
		pix = getTrayPixmap(!psi_.isNull() ? psi_->currentStatusType() : XMPP::Status::Offline,
		                    haveConnectingAccounts_);
	}

	setIcon(pix);
}

void YaTrayIcon::update()
{
	updateIcon();
}

void YaTrayIcon::setHaveConnectingAccounts(bool haveConnectingAccounts)
{
	haveConnectingAccounts_ = haveConnectingAccounts;
	updateIcon();
}

YaTrayIcon* YaTrayIcon::instance_ = 0;

#include "yatrayicon.moc"
