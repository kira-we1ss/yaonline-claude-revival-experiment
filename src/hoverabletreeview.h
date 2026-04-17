/*
 * hoverabletreeview.h - QTreeView that allows to show hovered items apart
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

#ifndef HOVERABLETREEVIEW_H
#define HOVERABLETREEVIEW_H

#include <QTreeView>
#include <QStyleOption>

typedef QStyleOptionViewItem HoverableStyleOptionViewItemBaseClass;

class HoverableStyleOptionViewItem : public HoverableStyleOptionViewItemBaseClass
{
public:
	enum StyleOptionVersion { Version = HoverableStyleOptionViewItemBaseClass::Version+1 };

	bool hovered;

	HoverableStyleOptionViewItem();
	HoverableStyleOptionViewItem(const HoverableStyleOptionViewItem &other)
		: HoverableStyleOptionViewItemBaseClass(Version)
	{
		*this = other;
	}
	// Define default copy-assign. Without it, Qt5 warns
	// -Wdeprecated-copy-with-user-provided-copy: the implicit copy-assign
	// is deprecated in the presence of a user-provided copy ctor. The
	// default does exactly the memberwise copy we want.
	HoverableStyleOptionViewItem &operator=(const HoverableStyleOptionViewItem &) = default;
	HoverableStyleOptionViewItem(const QStyleOptionViewItem &other);
	HoverableStyleOptionViewItem &operator = (const QStyleOptionViewItem &other);

protected:
	HoverableStyleOptionViewItem(int version);
};

class HoverableTreeView : public QTreeView
{
	Q_OBJECT

public:
	HoverableTreeView(QWidget* parent = 0);

	enum HoverableItemFeature {
		Hovered = 0x8000
	};

	void repairMouseTracking();

protected:
	// reimplemented
	void mouseMoveEvent(QMouseEvent* event);
	void drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const;
	void startDrag(Qt::DropActions supportedActions);
	void leaveEvent(QEvent*);

	QPoint mousePosition() const;

private:
	QPoint mousePosition_;
};

#endif
