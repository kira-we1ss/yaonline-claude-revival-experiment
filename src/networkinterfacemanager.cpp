/*
 * networkinterfacemanager.cpp
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

#include "networkinterfacemanager.h"

#include <QTimer>
#include <QNetworkInterface>

#include "psilogger.h"

NetworkInterfaceManager::NetworkInterfaceManager(QObject* parent)
	: QObject(parent)
{
	refreshTimer_ = new QTimer(this);
	refreshTimer_->setSingleShot(false);
	refreshTimer_->setInterval(1*1000);
	connect(refreshTimer_, SIGNAL(timeout()), SLOT(refresh()));
	refreshTimer_->start();

	blockSignals(true);
	refresh();
	blockSignals(false);
}

NetworkInterfaceManager::~NetworkInterfaceManager()
{
}

void NetworkInterfaceManager::refresh()
{
	bool emitSignal = false;
	QHash<QString, bool> upInterfaces;
	QList<QNetworkInterface> list = QNetworkInterface::allInterfaces();
	foreach(const QNetworkInterface& i, list) {
		if (i.flags().testFlag(QNetworkInterface::IsUp) &&
		    i.flags().testFlag(QNetworkInterface::IsRunning))
		{
			upInterfaces[i.name()] = true;
			if (!upInterfaces_.contains(i.name())) {
				emitSignal = true;
			}
		}
	}

	upInterfaces_ = upInterfaces;
	if (emitSignal) {
		emit networkInterfacesChanged();
	}

	if (PsiLogger::instance()->enableLogging()) {
		QStringList logText;
		foreach(const QNetworkInterface& i, list) {
			QList<QNetworkAddressEntry> addresses = i.addressEntries();
			QString addressesText;
			foreach(const QNetworkAddressEntry& e, addresses) {
				addressesText += e.ip().toString() + ";";
			}
			QString flags;
			flags += i.flags().testFlag(QNetworkInterface::IsUp) ? "IsUp;" : "";
			flags += i.flags().testFlag(QNetworkInterface::IsRunning) ? "IsRunning;" : "";
			flags += i.flags().testFlag(QNetworkInterface::CanBroadcast) ? "CanBroadcast;" : "";
			flags += i.flags().testFlag(QNetworkInterface::IsLoopBack) ? "IsLoopBack;" : "";
			flags += i.flags().testFlag(QNetworkInterface::IsPointToPoint) ? "IsPointToPoint;" : "";
			flags += i.flags().testFlag(QNetworkInterface::CanMulticast) ? "CanMulticast;" : "";

			QString text = QString("\tName: %1; Hardware Address: %2; Valid: %3; Flags: %4; Address: %5")
			               .arg(i.humanReadableName())
			               .arg(i.hardwareAddress())
			               .arg(i.isValid())
			               .arg(flags)
			               .arg(addressesText);
			logText << text;
		}

		if (previousInterfaces_ != logText) {
			PsiLogger::instance()->log(QString("netinterfaces:\n%1")
			                           .arg(logText.join("\n")));
			previousInterfaces_ = logText;
		}
	}
}
