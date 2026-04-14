/*
 * psinotifierbase.h
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

#ifndef PSINOTIFIERBASE_H
#define PSINOTIFIERBASE_H

#include <QObject>

class QStringList;
class PsiAccount;
class QPixmap;

#include "xmpp_jid.h"

class PsiNotifierBase : public QObject
{
	Q_OBJECT
public:
	PsiNotifierBase(QObject* parent, const QString& appName);
	virtual void notify(PsiAccount* account, const XMPP::Jid& jid, const QString& name, const QString& title, const QString& desc, const QPixmap& icon) = 0;

protected:
	QString appName() const;

private:
	QString appName_;
};


#endif
