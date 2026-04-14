/*
 * yarostertoolbutton.h
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

#ifndef YAROSTERTOOLBUTTON_H
#define YAROSTERTOOLBUTTON_H

#include <QToolButton>
#include <QPointer>

class YaRoster;

class YaRosterToolButton : public QToolButton
{
	Q_OBJECT
public:
	YaRosterToolButton(QWidget* parent);

	void setUnderMouse(bool underMouse);
	void setCompactSize(const QSize& size);

	// reimplemented
	QSize minimumSizeHint() const;
	QSize sizeHint() const;

	virtual bool pressable() const;

protected:
	// reimplemented
	void paintEvent(QPaintEvent*);
	void changeEvent(QEvent* e);

	virtual QIcon::Mode iconMode() const;
	virtual bool textVisible() const;

private:
	bool underMouse_;
	QSize compactSize_;
	QString toolTip_;
};

class YaRosterAddContactToolButton : public YaRosterToolButton
{
	Q_OBJECT
public:
	YaRosterAddContactToolButton(QWidget* parent);
	void init();

	// reimplemented
	virtual bool pressable() const;

	void setRoster(YaRoster* yaRoster);

protected:
	// reimplemented
	virtual QIcon::Mode iconMode() const;

private:
	QPointer<YaRoster> yaRoster_;
};

#endif
