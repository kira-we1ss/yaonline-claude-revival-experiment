/*
 * yamucmanager.h
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

#ifndef YAMUCMANAGER_H
#define YAMUCMANAGER_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QHash>

class PsiCon;
class PsiAccount;

#include "xmpp_discoitem.h"

class YaMucManager : public QObject
{
	Q_OBJECT
public:
	YaMucManager(PsiCon* controller);
	~YaMucManager();

	QStringList mucServers() const;

signals:
	void mucServersChanged() const;

private slots:
	void accountCountChanged();
	void rosterRequestFinished();
	void checkAccountForAvailableMucServers(PsiAccount* account);
	void discoItemsFinished();
	void discoInfoFinished();

protected:
	void processDiscoItemIdentities(const QString& domain, const XMPP::DiscoItem& discoItem);

private:
	QPointer<PsiCon> controller_;
	QHash<QString, QStringList> discoveredServers_;
	QStringList mucServersPre_;
	QStringList mucServersPost_;
};

#endif
