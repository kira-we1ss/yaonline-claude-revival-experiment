/*
 * yaaccountselector.h
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

#ifndef YAACCOUNTSELECTOR_H
#define YAACCOUNTSELECTOR_H

#include <QComboBox>
#include <QPointer>

class PsiCon;

class YaAccountSelector : public QComboBox
{
	Q_OBJECT
public:
	YaAccountSelector(QWidget* parent);
	~YaAccountSelector();

	void setController(PsiCon* controller);

	QString currentAccountId() const;

private slots:
	void accountCountChanged();

private:
	QPointer<PsiCon> controller_;
};

#endif
