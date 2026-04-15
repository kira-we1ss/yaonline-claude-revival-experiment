/*
 * gcuserview.h - groupchat roster
 * Copyright (C) 2001, 2002  Justin Karneges
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

#ifndef GCUSERVIEW_H
#define GCUSERVIEW_H

#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "xmpp_status.h"

using namespace XMPP;

class QPainter;
class PsiGroupchatDlg;
class GCUserView;
class GCUserViewGroupItem;
namespace XMPP {
	class Jid;
}

class GCUserViewItem : public QObject, public QTreeWidgetItem
{
public:
	GCUserViewItem(GCUserViewGroupItem *);

	Status s;

	bool operator<(const QTreeWidgetItem &other) const override;
};

class GCUserViewGroupItem : public QTreeWidgetItem
{
public:
	GCUserViewGroupItem(GCUserView *, const QString&, int);

	int key() const { return key_; }

	bool operator<(const QTreeWidgetItem &other) const override;

private:
	int key_;
};

class GCUserView : public QTreeWidget
{
	Q_OBJECT
public:
	GCUserView(QWidget* parent);
	~GCUserView();

	void setMainDlg(PsiGroupchatDlg* mainDlg);
	void clear();
	void updateAll();
	bool hasJid(const Jid&);
	QTreeWidgetItem *findEntry(const QString &);
	void updateEntry(const QString &, const Status &);
	void removeEntry(const QString &);
	QStringList nickList() const;

protected:
	enum Role { Moderator = 0, Participant = 1, Visitor = 2 };

	GCUserViewGroupItem* findGroup(XMPP::MUCItem::Role a) const;
	bool maybeTip(const QPoint &);
	bool event(QEvent* e) override;
	QMimeData *mimeData(const QList<QTreeWidgetItem *> items) const override;

signals:
	void action(const QString &, const Status &, int);

private slots:
	void qlv_doubleClicked(QTreeWidgetItem *);
	void qlv_contextMenuRequested(const QPoint &);

private:
	PsiGroupchatDlg* gcDlg_;
};

#endif
