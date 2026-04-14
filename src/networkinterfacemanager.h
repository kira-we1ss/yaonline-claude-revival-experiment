/*
 * networkinterfacemanager.h
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

#ifndef NETWORKINTERFACEMANAGER_H
#define NETWORKINTERFACEMANAGER_H

#include <QObject>
#include <QHash>
#include <QStringList>

class QTimer;

class NetworkInterfaceManager : public QObject
{
	Q_OBJECT
public:
	NetworkInterfaceManager(QObject* parent);
	~NetworkInterfaceManager();

signals:
	void networkInterfacesChanged();

private slots:
	void refresh();

private:
	QTimer* refreshTimer_;
	QHash<QString, bool> upInterfaces_;

	QStringList previousInterfaces_;
};

#endif
